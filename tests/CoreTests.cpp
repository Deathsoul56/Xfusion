#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

#include "CoreManifest.h"

class CoreTests : public QObject {
    Q_OBJECT

private slots:
    void aMissingManifestIsEmpty()
    {
        QTemporaryDir dir;
        QString error;
        const CoreManifest manifest = CoreManifest::load(dir.path(), &error);
        QVERIFY(error.isEmpty());
        QVERIFY(manifest.versions("picodrive").isEmpty());
        QVERIFY(manifest.activePath("picodrive").isEmpty());
    }

    void versionsAreKeptSideBySideAndTheActiveOneSurvivesARestart()
    {
        QTemporaryDir dir;
        CoreManifest manifest = CoreManifest::load(dir.path());
        manifest.add("genesis_plus_gx", {"v1.7.4 aaaa", "hash-a", "genesis_plus_gx/v1.7.4_aaaa/g.dll", "url", "2026-11-20T00:00:00Z", "2026-11-21T10:00:00Z"}, true);
        manifest.add("genesis_plus_gx", {"v1.7.4 bbbb", "hash-b", "genesis_plus_gx/v1.7.4_bbbb/g.dll", "url", "2026-11-21T00:00:00Z", "2026-11-22T10:00:00Z"}, true);
        QCOMPARE(manifest.versions("genesis_plus_gx").size(), 2);
        QCOMPARE(manifest.activeVersion("genesis_plus_gx"), QString("v1.7.4 bbbb"));
        QVERIFY(manifest.save());

        CoreManifest loaded = CoreManifest::load(dir.path());
        QCOMPARE(loaded.versions("genesis_plus_gx").size(), 2);
        QCOMPARE(loaded.activeVersion("genesis_plus_gx"), QString("v1.7.4 bbbb"));
        QCOMPARE(loaded.activePath("genesis_plus_gx"), QDir(dir.path()).absoluteFilePath("genesis_plus_gx/v1.7.4_bbbb/g.dll"));
        QCOMPARE(loaded.active("genesis_plus_gx")->sha256, QString("hash-b"));

        // Going back to the older version keeps the newer one installed.
        QVERIFY(loaded.setActive("genesis_plus_gx", "v1.7.4 aaaa"));
        QCOMPARE(loaded.versions("genesis_plus_gx").size(), 2);
        QVERIFY(!loaded.setActive("genesis_plus_gx", "v9"));
        loaded.clearActive("genesis_plus_gx");
        QVERIFY(loaded.activePath("genesis_plus_gx").isEmpty()); // the local file plays
    }

    void theActiveVersionCannotBeRemovedAndTheOthersCan()
    {
        QTemporaryDir dir;
        CoreManifest manifest = CoreManifest::load(dir.path());
        manifest.add("picodrive", {"1.0", "a", "p/1/p.dll", "", "", ""}, false);
        manifest.add("picodrive", {"2.0", "b", "p/2/p.dll", "", "", ""}, true);
        QVERIFY(!manifest.remove("picodrive", "2.0"));
        QVERIFY(manifest.remove("picodrive", "1.0"));
        QVERIFY(!manifest.remove("picodrive", "1.0"));
        QCOMPARE(manifest.versions("picodrive").size(), 1);
    }

    void aDamagedManifestIsReportedAndLeavesNothingActive()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath("manifest.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{ not json");
        file.close();
        QString error;
        const CoreManifest manifest = CoreManifest::load(dir.path(), &error);
        QVERIFY(!error.isEmpty());
        QVERIFY(manifest.activePath("picodrive").isEmpty());

        // An active version that is not in the list is dropped.
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"version\":1,\"cores\":{\"picodrive\":{\"active\":\"9.9\",\"versions\":[{\"version\":\"1.0\",\"path\":\"p/1/p.dll\"}]}}}");
        file.close();
        const CoreManifest odd = CoreManifest::load(dir.path());
        QVERIFY(odd.activeVersion("picodrive").isEmpty());
        QCOMPARE(odd.versions("picodrive").size(), 1);
    }

    void versionFoldersHaveSafeNames()
    {
        QCOMPARE(versionFolderName("v1.7.4 4fd32ef"), QString("v1.7.4_4fd32ef"));
        QCOMPARE(versionFolderName("../../evil\\x"), QString(".._.._evil_x"));
        QCOMPARE(versionFolderName("   "), QString("unknown"));
    }
};

QTEST_MAIN(CoreTests)
#include "CoreTests.moc"
