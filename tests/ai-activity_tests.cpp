#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QUuid>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFileDevice>
#include <memory>
#include <utility>

#include "storage.h"
#include "trmclient.h"
#include "theme.h"
#include "backend.h"
#include "parsers/pishapedparser.h"
#include "parsers/claudeparser.h"
#include "parsers/sourceregistry.h"

namespace {

// Wraps a real parser so incremental tests can count how many files were
// actually parsed (skipped files never show up in ParseResult::files).
class CountingParser : public UsageParser {
public:
    CountingParser(std::shared_ptr<UsageParser> delegate, int *filesParsed)
        : m_delegate(std::move(delegate)), m_filesParsed(filesParsed) {}

    QString source() const override { return m_delegate->source(); }

    ParseResult parse(const QString &rootPath, const FileSkipPredicate &shouldSkip) const override {
        ParseResult result = m_delegate->parse(rootPath, shouldSkip);
        *m_filesParsed += result.files.size();
        return result;
    }

private:
    std::shared_ptr<UsageParser> m_delegate;
    int *m_filesParsed;
};

int countEventRows(Storage *storage) {
    QSqlQuery q(QSqlDatabase::database(storage->connectionName()));
    if (!q.exec("SELECT COUNT(*) FROM events;"))
        return -1;
    return q.next() ? q.value(0).toInt() : -1;
}

}  // namespace

class AiActivityTests : public QObject {
    Q_OBJECT

private slots:
    void testProjectNameExtraction();
    void testProjectNameExtractionPortable();
    void testStorageInitAndParsing();
    void testTrmConversion();
    void testTrmAutoSyncOptOut();
    void testThemeAccentDefault();
    void testThemeScaleAndDp();
    void testStreakCalculation();
    void testNumberFormatting();
    void testRangeSwitching();
    void testQmlLoadsCleanly();
    void testLegacyDatabaseMigration();
    void testSourceScopedMessageId();
    void testOmpParser();
    void testClaudeParser();
    void testIncrementalSync();
    void testRegistryDisabledSources();
    void testBackendSourceWiring();
};

void AiActivityTests::testProjectNameExtraction() {
    QCOMPARE(Storage::extractProjectFromDirName("--home-juan-Work--"), QString("Work"));
    QCOMPARE(Storage::extractProjectFromDirName("--home-juan-Projects-ai-activity--"), QString("Projects/ai-activity"));
    QCOMPARE(Storage::extractProjectFromDirName("--home-juan--"), QString("Home"));
    QVERIFY(Storage::extractProjectFromDirName("--home-juan-OpenBot-Agents-agent-123--").contains("Agent"));
}

void AiActivityTests::testProjectNameExtractionPortable() {
    const QByteArray originalHome = qgetenv("HOME");
    qputenv("HOME", "/home/portableuser");

    const QString homeName = QFileInfo(QDir::homePath()).fileName();
    const QString homeDir = Storage::extractProjectFromDirName(QString("--home-%1--").arg(homeName));
    const QString projectDir = Storage::extractProjectFromDirName(
        QString("--home-%1-Projects-ai-activity--").arg(homeName));

    if (originalHome.isEmpty())
        qunsetenv("HOME");
    else
        qputenv("HOME", originalHome);

    QCOMPARE(homeName, QString("portableuser"));
    QCOMPARE(homeDir, QString("Home"));
    QCOMPARE(projectDir, QString("Projects/ai-activity"));
}

void AiActivityTests::testStorageInitAndParsing() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString dbPath = tempDir.path() + "/test_usage.db";
    Storage storage(dbPath);
    QVERIFY(storage.init());

    // Create a mock Pi session directory structure
    const QString sessionsDir = tempDir.path() + "/sessions";
    const QString mockSessionDir = sessionsDir + "/--home-juan-Projects-testproject--";
    QDir().mkpath(mockSessionDir);

    QFile jsonlFile(mockSessionDir + "/session1.jsonl");
    QVERIFY(jsonlFile.open(QIODevice::WriteOnly | QIODevice::Text));

    // Write 2 synthetic messages: 1 user, 1 assistant with usage & thinking
    const QString userLine = "{\"type\":\"message\",\"id\":\"m1\",\"timestamp\":1700000000000,\"message\":{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"hello\"}]}}\n";
    const QString assistantLine = "{\"type\":\"message\",\"id\":\"m2\",\"timestamp\":1700000005000,\"model\":\"gemini-2.5-pro\",\"provider\":\"antigravity\",\"message\":{\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"hi\"}]},\"usage\":{\"input\":100,\"output\":50,\"reasoning\":30,\"totalTokens\":180,\"cost\":{\"total\":0.0015}}}\n";

    jsonlFile.write(userLine.toUtf8());
    jsonlFile.write(assistantLine.toUtf8());
    jsonlFile.close();

    // Sync from mock sessions
    int synced = storage.syncFromPiSessions(sessionsDir);
    QCOMPARE(synced, 1);

    // Verify deduplication on re-sync
    int resynced = storage.syncFromPiSessions(sessionsDir);
    QCOMPARE(resynced, 0);

    // Verify summary
    UsageSummary summary = storage.getSummary(0, 1800000000000);
    QCOMPARE(summary.totalTurns, 1);
    QCOMPARE(summary.totalTokens, 180);
    QCOMPARE(summary.inputTokens, 100);
    QCOMPARE(summary.outputTokens, 50);
    QCOMPARE(summary.thinkingTokens, 30);
    QCOMPARE(summary.totalCostUsd, 0.0015);

    // Verify breakdown
    QVariantList projectBreakdown = storage.getBreakdown("project", 0, 1800000000000);
    QCOMPARE(projectBreakdown.size(), 1);
    QCOMPARE(projectBreakdown[0].toMap()["name"].toString(), QString("Projects/testproject"));

    QVariantList modelBreakdown = storage.getBreakdown("model", 0, 1800000000000);
    QCOMPARE(modelBreakdown.size(), 1);
    QCOMPARE(modelBreakdown[0].toMap()["name"].toString(), QString("gemini-2.5-pro"));
}

void AiActivityTests::testTrmConversion() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString trmCache = tempDir.path() + "/trm.json";
    QFile f(trmCache);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("{\"rate\":4200.0,\"timestamp\":\"2026-01-01T00:00:00\"}");
    f.close();

    TrmClient client(trmCache, nullptr, false);
    QCOMPARE(client.rate(), 4200.0);
    QCOMPARE(client.convertUsdToCop(10.0), 42000.0);
}

void AiActivityTests::testTrmAutoSyncOptOut() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString trmCache = tempDir.path() + "/trm.json";
    QFile f(trmCache);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("{\"rate\":4200.0,\"timestamp\":\"2026-01-01T00:00:00\"}");
    f.close();

    // Opt-out keeps the cached rate and starts no request even when the cache is stale.
    TrmClient client(trmCache, nullptr, false);
    QCOMPARE(client.rate(), 4200.0);
    QVERIFY(!client.isSyncing());

    // Opt-out keeps the offline default when no cache exists.
    TrmClient fresh(tempDir.path() + "/missing.json", nullptr, false);
    QCOMPARE(fresh.rate(), 4100.0);
    QVERIFY(!fresh.isSyncing());

    // Backend threads the same opt-out through to its TRM client.
    Backend backend(tempDir.path() + "/test_optout.db", tempDir.path(), nullptr, false);
    QVERIFY(!backend.isSyncing());
}

void AiActivityTests::testThemeAccentDefault() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    Theme theme(tempDir.path());
    // Should have valid default accent and foreground
    QVERIFY(theme.accent().startsWith("#"));
    QVERIFY(theme.accentForeground() == "black" || theme.accentForeground() == "white");
}

void AiActivityTests::testThemeScaleAndDp() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    Theme theme(tempDir.path());
    QCOMPARE(theme.uiScale(), 1.18);
    QCOMPARE(theme.dp(100), 118);
    QCOMPARE(theme.dp(10), 12);
    QCOMPARE(theme.dp(14), 17);

    // Test setter and clamp bounds
    theme.setUiScale(1.5);
    QCOMPARE(theme.uiScale(), 1.5);
    QCOMPARE(theme.dp(10), 15);

    // Out of bounds should be rejected
    theme.setUiScale(0.2);
    QCOMPARE(theme.uiScale(), 1.5);
    theme.setUiScale(5.0);
    QCOMPARE(theme.uiScale(), 1.5);
}

void AiActivityTests::testStreakCalculation() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    Storage storage(tempDir.path() + "/test_streak.db");
    QVERIFY(storage.init());

    const QDate today(2026, 11, 11);

    // Empty DB: 0 streak
    auto s0 = storage.getStreak(today);
    QCOMPARE(s0.currentStreak, 0);
    QCOMPARE(s0.maxStreak, 0);

    // Helper to insert a mock event at a given date
    auto insertEvent = [&](const QString &dateStr, const QString &id) {
        QSqlQuery q(QSqlDatabase::database(storage.connectionName()));
        q.prepare(R"(
            INSERT INTO events (session_id, message_id, timestamp, date, project, agent, model, provider, tokens_in, tokens_out, tokens_reasoning, tokens_total, cost_usd, is_subagent)
            VALUES ('s1', :id, 1000, :date, 'p1', 'a1', 'm1', 'pr1', 10, 10, 0, 20, 0.001, 0);
        )");
        q.bindValue(":id", id);
        q.bindValue(":date", dateStr);
        QVERIFY(q.exec());
    };

    // Insert dates: 2026-11-01, 2026-11-02, 2026-11-03 (run of 3)
    insertEvent("2026-11-01", "m1");
    insertEvent("2026-11-02", "m2");
    insertEvent("2026-11-03", "m3");

    // Insert dates: 2026-11-09, 2026-11-10 (run of 2, ending yesterday relative to 2026-11-11)
    insertEvent("2026-11-09", "m4");
    insertEvent("2026-11-10", "m5");

    auto s1 = storage.getStreak(today);
    // Yesterday had activity, so current streak is 2, longest streak is 3 (Nov 1-3)
    QCOMPARE(s1.currentStreak, 2);
    QCOMPARE(s1.maxStreak, 3);

    // Add activity today (2026-11-11)
    insertEvent("2026-11-11", "m6");
    auto s2 = storage.getStreak(today);
    QCOMPARE(s2.currentStreak, 3);
    QCOMPARE(s2.maxStreak, 3);

    // Add activity tomorrow (2026-11-12) with refDate tomorrow -> streak becomes 4 (Nov 9, 10, 11, 12)
    insertEvent("2026-11-12", "m7");
    auto s3 = storage.getStreak(QDate(2026, 11, 12));
    QCOMPARE(s3.currentStreak, 4);
    QCOMPARE(s3.maxStreak, 4);
}

void AiActivityTests::testNumberFormatting() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    Backend backend(tempDir.path() + "/test_fmt.db", tempDir.path(), nullptr, false);

    // Test compact formatting without scientific notation
    QCOMPARE(backend.formatCompact(363073000), QString("363.07M"));
    QCOMPARE(backend.formatCompact(1504500), QString("1.50M"));
    QCOMPARE(backend.formatCompact(18512000), QString("18.51M"));
    QCOMPARE(backend.formatCompact(1250000000), QString("1.25B"));
    QCOMPARE(backend.formatCompact(5400), QString("5.4k"));
    QCOMPARE(backend.formatCompact(42), QString("42"));

    // Ensure formatNumber produces separated numbers, not e+08
    QString formatted = backend.formatNumber(363073000);
    QVERIFY(!formatted.contains("e+"));
    QVERIFY(formatted.contains("363"));
}

void AiActivityTests::testRangeSwitching() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    Backend backend(tempDir.path() + "/test_range.db", tempDir.path(), nullptr, false);
    QCOMPARE(backend.currentRange(), QString("7d"));

    // Test Q_INVOKABLE setRange
    backend.setRange("5h");
    QCOMPARE(backend.currentRange(), QString("5h"));

    backend.setRange("30d");
    QCOMPARE(backend.currentRange(), QString("30d"));

    backend.setRange("all");
    QCOMPARE(backend.currentRange(), QString("all"));

    // Test Q_PROPERTY write
    backend.setProperty("currentRange", "12h");
    QCOMPARE(backend.currentRange(), QString("12h"));
}

void AiActivityTests::testQmlLoadsCleanly() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    Theme theme(tempDir.path());
    Backend backend(tempDir.path() + "/test_qml.db", tempDir.path(), nullptr, false);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);

    engine.load(QUrl("qrc:/Main.qml"));
    QVERIFY2(!engine.rootObjects().isEmpty(), "QQmlApplicationEngine failed to load qrc:/Main.qml");
}

void AiActivityTests::testLegacyDatabaseMigration() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString dbPath = tempDir.path() + "/legacy.db";

    // Build an old-format database on a dedicated connection name so it does not
    // clash with the connection Storage opens when it migrates the same file.
    const QString setupConn = QUuid::createUuid().toString();
    {
        QSqlDatabase legacy = QSqlDatabase::addDatabase("QSQLITE", setupConn);
        legacy.setDatabaseName(dbPath);
        QVERIFY(legacy.open());

        QSqlQuery create(legacy);
        QVERIFY2(create.exec(R"(
            CREATE TABLE events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                session_id TEXT,
                message_id TEXT UNIQUE,
                timestamp INTEGER, date TEXT, project TEXT, agent TEXT,
                model TEXT, provider TEXT,
                tokens_in INTEGER, tokens_out INTEGER, tokens_reasoning INTEGER,
                tokens_total INTEGER, cost_usd REAL, is_subagent INTEGER
            );
        )"), qPrintable(create.lastError().text()));

        for (const QString &id : {QString("legacy-m1"), QString("legacy-m2")}) {
            QSqlQuery insert(legacy);
            insert.prepare(R"(
                INSERT INTO events (session_id, message_id, timestamp, date, project, agent, tokens_total)
                VALUES ('s1', :id, 1000, '2026-11-01', 'p1', 'pi', 10);
            )");
            insert.bindValue(":id", id);
            QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
        }
    }
    QSqlDatabase::removeDatabase(setupConn);

    // Opening through Storage must migrate the table without losing either row
    // and must backfill the new source column with 'pi'.
    Storage storage(dbPath);
    QVERIFY(storage.init());

    QSqlQuery check(QSqlDatabase::database(storage.connectionName()));
    QVERIFY2(check.exec("SELECT message_id, source FROM events ORDER BY message_id;"),
             qPrintable(check.lastError().text()));

    QStringList ids;
    int piRows = 0;
    while (check.next()) {
        ids << check.value("message_id").toString();
        if (check.value("source").toString() == "pi")
            piRows++;
    }
    QCOMPARE(ids.size(), 2);
    QCOMPARE(piRows, 2);
    QVERIFY(ids.contains("legacy-m1"));
    QVERIFY(ids.contains("legacy-m2"));

    // The migration must also recreate the original indexes and add sync_state.
    QSqlQuery indexes(QSqlDatabase::database(storage.connectionName()));
    QVERIFY(indexes.exec("SELECT name FROM sqlite_master WHERE type = 'index';"));
    QStringList indexNames;
    while (indexes.next())
        indexNames << indexes.value("name").toString();
    QVERIFY(indexNames.contains("idx_events_timestamp"));
    QVERIFY(indexNames.contains("idx_events_date"));
    QVERIFY(indexNames.contains("idx_events_project"));
    QVERIFY(indexNames.contains("idx_events_model"));

    QSqlQuery tables(QSqlDatabase::database(storage.connectionName()));
    QVERIFY(tables.exec("SELECT name FROM sqlite_master WHERE type = 'table' AND name = 'sync_state';"));
    QVERIFY(tables.next());
}

void AiActivityTests::testSourceScopedMessageId() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    Storage storage(tempDir.path() + "/source.db");
    QVERIFY(storage.init());
    QSqlDatabase db = QSqlDatabase::database(storage.connectionName());

    auto insert = [&db](const QString &source, const QString &messageId) {
        QSqlQuery q(db);
        q.prepare("INSERT INTO events (source, message_id, timestamp, date) "
                  "VALUES (:source, :message_id, 1000, '2026-11-01');");
        q.bindValue(":source", source);
        q.bindValue(":message_id", messageId);
        return q.exec();
    };

    // The same message_id can live under two different sources.
    QVERIFY2(insert("pi", "shared-id"), "pi/omp dedupe must be source-scoped");
    QVERIFY2(insert("omp", "shared-id"), "pi/omp dedupe must be source-scoped");

    // But the exact (source, message_id) pair is still rejected.
    QVERIFY(!insert("pi", "shared-id"));
}

void AiActivityTests::testOmpParser() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString root = tempDir.path() + "/omp";
    const QString mainDir = root + "/--home-juan-Projects-oma-type--";
    const QString subagentDir = root + "/--home-juan-gentle-agents-agent-7--";
    QVERIFY(QDir().mkpath(mainDir));
    QVERIFY(QDir().mkpath(subagentDir));

    // omp records are Pi-shaped: usage lives under .message.usage, reasoning is
    // named reasoningTokens, and the model/provider/ttft fields sit at root.
    const QByteArray mainLine =
        "{\"type\":\"message\",\"id\":\"omp-m1\",\"timestamp\":1700000000000,"
        "\"api\":\"anthropic\",\"modelID\":\"claude-sonnet-4\",\"providerID\":\"anthropic\","
        "\"ttft\":120,\"message\":{\"role\":\"assistant\",\"content\":[]},"
        "\"usage\":{\"input\":100,\"output\":50,\"reasoningTokens\":25,"
        "\"totalTokens\":175,\"cost\":{\"total\":0.0025}}}\n";
    QFile mainFile(mainDir + "/session-a.jsonl");
    QVERIFY(mainFile.open(QIODevice::WriteOnly | QIODevice::Text));
    mainFile.write(mainLine);
    mainFile.close();

    const QByteArray subLine =
        "{\"type\":\"message\",\"id\":\"omp-m2\",\"timestamp\":1700000005000,"
        "\"api\":\"openai\",\"modelID\":\"gpt-x\",\"providerID\":\"openai\","
        "\"message\":{\"role\":\"assistant\",\"content\":[]},"
        "\"usage\":{\"input\":10,\"output\":5,\"reasoningTokens\":2,"
        "\"totalTokens\":17,\"cost\":{\"total\":0.0003}}}\n";
    QFile subFile(subagentDir + "/session-b.jsonl");
    QVERIFY(subFile.open(QIODevice::WriteOnly | QIODevice::Text));
    subFile.write(subLine);
    subFile.close();

    OmpParser parser;
    QCOMPARE(parser.source(), QString("omp"));
    const ParseResult result = parser.parse(root, nullptr);
    QCOMPARE(result.events.size(), 2);
    QCOMPARE(result.files.size(), 2);

    auto findEvent = [&result](const QString &id) {
        for (const NormalizedEvent &event : result.events) {
            if (event.messageId == id)
                return event;
        }
        return NormalizedEvent();
    };

    const NormalizedEvent main = findEvent("omp-m1");
    QCOMPARE(main.source, QString("omp"));
    QCOMPARE(main.sessionId, QString("session-a"));
    QCOMPARE(main.project, QString("Projects/oma-type"));
    QCOMPARE(main.agent, QString("omp"));
    QCOMPARE(main.model, QString("claude-sonnet-4"));
    QCOMPARE(main.provider, QString("anthropic"));
    QCOMPARE(main.tokensIn, 100LL);
    QCOMPARE(main.tokensOut, 50LL);
    QCOMPARE(main.tokensReasoning, 25LL);
    QCOMPARE(main.tokensTotal, 175LL);
    QCOMPARE(main.costUsd, 0.0025);
    QCOMPARE(main.timestampMs, 1700000000000LL);
    QVERIFY(!main.isSubagent);

    const NormalizedEvent sub = findEvent("omp-m2");
    QCOMPARE(sub.project, QString("Agent (7)"));
    QVERIFY(sub.isSubagent);
    QCOMPARE(sub.tokensReasoning, 2LL);
}

void AiActivityTests::testClaudeParser() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString root = tempDir.path() + "/claude";
    const QString projectDir = root + "/projects-oma-type";
    QVERIFY(QDir().mkpath(projectDir + "/subagents"));

    const QString cwd = QDir::homePath() + "/Projects/oma-type";

    auto assistantLine = [&cwd](const QString &uuid, const QString &msgId, const QString &ts,
                                int inputTokens, int outputTokens, int cacheWrite, int cacheRead,
                                int thinking, const QString &sessionId = QString("sess-1"),
                                const QString &extra = QString()) {
        return QString("{\"type\":\"assistant\",\"uuid\":\"%1\",\"sessionId\":\"%2\","
                       "\"timestamp\":\"%3\",\"cwd\":\"%4\","
                       "\"message\":{\"id\":\"%5\",\"role\":\"assistant\","
                       "\"model\":\"claude-sonnet-4\",\"usage\":{\"input_tokens\":%6,"
                       "\"output_tokens\":%7,\"cache_creation_input_tokens\":%8,"
                       "\"cache_read_input_tokens\":%9,"
                       "\"output_tokens_details\":{\"thinking_tokens\":%10}}},"
                       "\"costUSD\":null%11}")
            .arg(uuid, sessionId, ts, cwd, msgId)
            .arg(inputTokens).arg(outputTokens).arg(cacheWrite).arg(cacheRead)
            .arg(thinking).arg(extra);
    };

    QStringList sessionLines;
    sessionLines << assistantLine("uuid-1", "msg-a1", "2026-11-17T10:15:30.123Z", 100, 50, 20, 10, 15);
    sessionLines << assistantLine("uuid-2", "msg-a2", "2026-11-17T10:16:30.123Z", 200, 100, 0, 0, 25);
    // Same uuid with a different message.id must be deduped by uuid, not message.id.
    sessionLines << assistantLine("uuid-1", "msg-dup", "2026-11-17T10:17:30.123Z", 999, 999, 999, 999, 999);
    sessionLines << assistantLine("uuid-3", "msg-side", "2026-11-17T10:18:30.123Z", 10, 5, 0, 0, 2,
                                  "sess-2", ",\"agentId\":\"agent-7\",\"isSidechain\":true");
    sessionLines << QString("{\"type\":\"cost-state\",\"totalCostUSD\":0.09}");
    QFile sessionFile(projectDir + "/session.jsonl");
    QVERIFY(sessionFile.open(QIODevice::WriteOnly | QIODevice::Text));
    for (const QString &line : sessionLines)
        sessionFile.write((line + "\n").toUtf8());
    sessionFile.close();

    // A nested subagent file with no cost-state: its cost must stay zero.
    QFile subFile(projectDir + "/subagents/agent-session.jsonl");
    QVERIFY(subFile.open(QIODevice::WriteOnly | QIODevice::Text));
    subFile.write((assistantLine("uuid-4", "msg-sub", "2026-11-17T10:19:30.123Z",
                                 40, 20, 0, 0, 5, "sess-3") + "\n").toUtf8());
    subFile.close();

    ClaudeParser parser;
    QCOMPARE(parser.source(), QString("claude"));
    const ParseResult result = parser.parse(root, nullptr);
    QCOMPARE(result.events.size(), 4);

    auto findEvent = [&result](const QString &id) {
        for (const NormalizedEvent &event : result.events) {
            if (event.messageId == id)
                return event;
        }
        return NormalizedEvent();
    };

    const NormalizedEvent first = findEvent("uuid-1");
    QCOMPARE(first.source, QString("claude"));
    QCOMPARE(first.sessionId, QString("sess-1"));
    QCOMPARE(first.project, QString("Projects/oma-type"));
    QCOMPARE(first.model, QString("claude-sonnet-4"));
    QCOMPARE(first.tokensIn, 130LL);   // input + cache write + cache read
    QCOMPARE(first.tokensOut, 50LL);
    QCOMPARE(first.tokensReasoning, 15LL);
    QCOMPARE(first.tokensTotal, 195LL);
    QVERIFY(!first.isSubagent);
    QCOMPARE(first.timestampMs,
             QDateTime::fromString("2026-11-17T10:15:30.123Z", Qt::ISODateWithMs).toMSecsSinceEpoch());

    // uuid dedupe: the duplicate uuid's message.id must never appear.
    for (const NormalizedEvent &event : result.events)
        QVERIFY(event.messageId != QString("msg-dup"));

    const NormalizedEvent side = findEvent("uuid-3");
    QVERIFY(side.isSubagent);
    QVERIFY(side.costUsd > 0.0);

    const NormalizedEvent nested = findEvent("uuid-4");
    QCOMPARE(nested.sessionId, QString("sess-3"));
    QCOMPARE(nested.costUsd, 0.0);

    // Prorated costs in the main session file sum back to totalCostUSD.
    double proratedSum = 0.0;
    for (const NormalizedEvent &event : result.events) {
        if (event.sessionId == "sess-1" || event.sessionId == "sess-2")
            proratedSum += event.costUsd;
    }
    QVERIFY(qAbs(proratedSum - 0.09) < 1e-9);
}

void AiActivityTests::testIncrementalSync() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    Storage storage(tempDir.path() + "/sync.db");
    QVERIFY(storage.init());

    const QString root = tempDir.path() + "/omp";
    const QString sessionDir = root + "/--home-juan-Projects-incremental--";
    QVERIFY(QDir().mkpath(sessionDir));
    const QString filePath = sessionDir + "/session.jsonl";

    auto writeLine = [&filePath](const QString &id) {
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write((QString("{\"type\":\"message\",\"id\":\"%1\",\"timestamp\":1700000000000,"
                            "\"message\":{\"role\":\"assistant\"},"
                            "\"usage\":{\"input\":10,\"output\":5,\"reasoningTokens\":2,"
                            "\"totalTokens\":17,\"cost\":{\"total\":0.001}}}\n").arg(id)).toUtf8());
        file.close();
    };
    writeLine("inc-m1");

    int filesParsed = 0;
    auto counting = std::make_shared<CountingParser>(std::make_shared<OmpParser>(), &filesParsed);

    SourceRegistry registry(&storage);
    registry.setEnabled("pi", false);
    registry.setEnabled("claude", false);
    registry.setEnabled("omp", true);
    registry.setRootPath("omp", root);
    registry.setParser("omp", counting);

    QCOMPARE(registry.syncAll(), 1);
    QCOMPARE(filesParsed, 1);
    QCOMPARE(countEventRows(&storage), 1);

    // Unchanged mtime/size: the file is skipped and nothing is inserted.
    QCOMPARE(registry.syncAll(), 0);
    QCOMPARE(filesParsed, 1);
    QCOMPARE(countEventRows(&storage), 1);

    // Touching only the mtime must force a re-parse (content stays identical,
    // so the source-scoped dedupe still keeps a single row).
    QFile touchFile(filePath);
    QVERIFY(touchFile.open(QIODevice::ReadOnly));
    QVERIFY2(touchFile.setFileTime(QDateTime::currentDateTimeUtc().addSecs(5),
                                   QFileDevice::FileModificationTime),
             "failed to touch fixture mtime");
    touchFile.close();
    QCOMPARE(registry.syncAll(), 0);
    QCOMPARE(filesParsed, 2);
    QCOMPARE(countEventRows(&storage), 1);

    // A changed file with a new message id produces a new row.
    writeLine("inc-m2");
    QCOMPARE(registry.syncAll(), 1);
    QCOMPARE(filesParsed, 3);
    QCOMPARE(countEventRows(&storage), 2);
}

void AiActivityTests::testRegistryDisabledSources() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    Storage storage(tempDir.path() + "/disabled.db");
    QVERIFY(storage.init());

    const QString root = tempDir.path() + "/omp";
    const QString sessionDir = root + "/--home-juan-Projects-disabled--";
    QVERIFY(QDir().mkpath(sessionDir));
    QFile file(sessionDir + "/session.jsonl");
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("{\"type\":\"message\",\"id\":\"disabled-m1\",\"timestamp\":1700000000000,"
               "\"message\":{\"role\":\"assistant\"},"
               "\"usage\":{\"input\":1,\"output\":1,\"reasoningTokens\":0,"
               "\"totalTokens\":2,\"cost\":{\"total\":0.001}}}\n");
    file.close();

    int filesParsed = 0;
    auto counting = std::make_shared<CountingParser>(std::make_shared<OmpParser>(), &filesParsed);

    SourceRegistry registry(&storage);
    registry.setEnabled("pi", false);
    registry.setEnabled("claude", false);
    registry.setEnabled("omp", false);
    registry.setRootPath("omp", root);
    registry.setParser("omp", counting);

    QVERIFY(!registry.isEnabled("omp"));
    QCOMPARE(registry.syncAll(), 0);
    QCOMPARE(filesParsed, 0);
    QCOMPARE(countEventRows(&storage), 0);
}

void AiActivityTests::testBackendSourceWiring() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    // Redirect HOME so "real" source roots are synthetic fixtures, never the
    // user's ~/.pi, ~/.omp or ~/.claude directories.
    const QByteArray originalHome = qgetenv("HOME");
    const QString home = tempDir.path();
    qputenv("HOME", home.toUtf8());

    auto writeRecord = [](const QString &path, const QByteArray &line) {
        QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(line);
        file.close();
    };

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const QByteArray piLine = QString("{\"type\":\"message\",\"id\":\"wired-pi\",\"timestamp\":%1,"
                                      "\"message\":{\"role\":\"assistant\"},"
                                      "\"usage\":{\"input\":1,\"output\":1,\"reasoning\":0,"
                                      "\"totalTokens\":2,\"cost\":{\"total\":0.001}}}\n")
                                  .arg(nowMs).toUtf8();
    const QByteArray ompLine = QString("{\"type\":\"message\",\"id\":\"wired-omp\",\"timestamp\":%1,"
                                       "\"modelID\":\"m\",\"providerID\":\"p\","
                                       "\"message\":{\"role\":\"assistant\"},"
                                       "\"usage\":{\"input\":1,\"output\":1,\"reasoningTokens\":0,"
                                       "\"totalTokens\":2,\"cost\":{\"total\":0.001}}}\n")
                                   .arg(nowMs).toUtf8();
    const QByteArray claudeLine = QString("{\"type\":\"assistant\",\"uuid\":\"wired-claude\","
                                          "\"sessionId\":\"s\",\"timestamp\":\"%1\","
                                          "\"message\":{\"role\":\"assistant\",\"model\":\"m\","
                                          "\"usage\":{\"input_tokens\":1,\"output_tokens\":1}}}\n"
                                          "{\"type\":\"cost-state\",\"totalCostUSD\":0.01}\n")
                                      .arg(QDateTime::fromMSecsSinceEpoch(nowMs).toUTC().toString(Qt::ISODateWithMs))
                                      .toUtf8();

    writeRecord(home + "/.pi/agent/sessions/--home-juan-Projects-pi--/pi.jsonl", piLine);
    writeRecord(home + "/.omp/agent/sessions/--home-juan-Projects-omp--/omp.jsonl", ompLine);
    writeRecord(home + "/.claude/projects/slug/session.jsonl", claudeLine);

    // Production-like Backend: no custom dir means all three default sources.
    Backend full(home + "/full.db", QString(), nullptr, false);
    QStringList fullAgents;
    for (const QVariant &item : full.agentBreakdown())
        fullAgents << item.toMap()["name"].toString();

    // Custom-dir Backend: only Pi, rooted at the injected directory.
    Backend hermetic(home + "/hermetic.db", home + "/.pi/agent/sessions", nullptr, false);
    QStringList hermeticAgents;
    for (const QVariant &item : hermetic.agentBreakdown())
        hermeticAgents << item.toMap()["name"].toString();

    if (originalHome.isEmpty())
        qunsetenv("HOME");
    else
        qputenv("HOME", originalHome);

    QVERIFY(fullAgents.contains("pi"));
    QVERIFY(fullAgents.contains("omp"));
    QVERIFY(fullAgents.contains("claude"));
    QVERIFY(hermeticAgents.contains("pi"));
    QVERIFY(!hermeticAgents.contains("omp"));
    QVERIFY(!hermeticAgents.contains("claude"));
}

QTEST_MAIN(AiActivityTests)
#include "ai-activity_tests.moc"
