#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "storage.h"
#include "trmclient.h"
#include "theme.h"
#include "backend.h"

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

QTEST_MAIN(AiActivityTests)
#include "ai-activity_tests.moc"
