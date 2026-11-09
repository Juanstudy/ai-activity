#include <QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "storage.h"
#include "trmclient.h"
#include "theme.h"
#include "backend.h"

class AiActivityTests : public QObject {
    Q_OBJECT

private slots:
    void testProjectNameExtraction();
    void testStorageInitAndParsing();
    void testTrmConversion();
    void testThemeAccentDefault();
    void testNumberFormatting();
};

void AiActivityTests::testProjectNameExtraction() {
    QCOMPARE(Storage::extractProjectFromDirName("--home-juan-Work--"), QString("Work"));
    QCOMPARE(Storage::extractProjectFromDirName("--home-juan-Projects-ai-activity--"), QString("Projects/ai-activity"));
    QCOMPARE(Storage::extractProjectFromDirName("--home-juan--"), QString("Home"));
    QVERIFY(Storage::extractProjectFromDirName("--home-juan-OpenBot-Agents-agent-123--").contains("Agent"));
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

    TrmClient client(trmCache);
    QCOMPARE(client.rate(), 4200.0);
    QCOMPARE(client.convertUsdToCop(10.0), 42000.0);
}

void AiActivityTests::testThemeAccentDefault() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    Theme theme(tempDir.path());
    // Should have valid default accent and foreground
    QVERIFY(theme.accent().startsWith("#"));
    QVERIFY(theme.accentForeground() == "black" || theme.accentForeground() == "white");
}

void AiActivityTests::testNumberFormatting() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    Backend backend(tempDir.path() + "/test_fmt.db", tempDir.path());

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

QTEST_MAIN(AiActivityTests)
#include "ai-activity_tests.moc"
