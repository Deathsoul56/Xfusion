#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

#include "CoreProfile.h"
#include "CoreRules.h"
#include "Library.h"

namespace {

void writeFile(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(path));
    file.write(bytes);
}

// The first sectors of a Sega CD data track, as raw 2352-byte sectors or cooked 2048-byte ones.
QByteArray discTrack(bool raw, const QByteArray& region = "U", const QByteArray& title = "TEST GAME")
{
    QByteArray user(2048, ' ');
    user.replace(0, 14, "SEGADISCSYSTEM");
    user.replace(0x10, 11, "TESTDISC   ");
    user.replace(0x100, 15, "SEGA MEGA DRIVE");
    user.replace(0x120, title.size(), title);
    user.replace(0x150, title.size(), title);
    user.replace(0x180, 14, "GM MK-4407 -00");
    user.replace(0x1F0, region.size(), region);
    if (!raw)
        return user + QByteArray(2048, 'x');
    QByteArray sector = QByteArray("\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x02\x00\x01", 16) + user + QByteArray(288, 0);
    return sector + QByteArray(2352, 'x');
}

QStringList names(const LibraryScan& scan)
{
    QStringList list;
    for (const LibraryEntry& entry : scan.entries)
        list << entry.name;
    return list;
}

}

class LibraryTests : public QObject {
    Q_OBJECT

private slots:
    void findsGamesInSubfoldersAndIgnoresOtherFiles()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("Sonic.md"), "sonic");
        writeFile(dir.filePath("deep/er/Columns.gg"), "columns");
        writeFile(dir.filePath("Alex Kidd.sms"), "alex");
        writeFile(dir.filePath("readme.txt"), "no");
        writeFile(dir.filePath("cover.png"), "no");
        const LibraryScan scan = scanLibrary({dir.path()}, {});
        QCOMPARE(names(scan), QStringList({"Alex Kidd", "Columns", "Sonic"}));
        QCOMPARE(scan.entries[1].system, System::GameGear);
        QCOMPARE(scan.entries[2].system, System::Genesis);
        QVERIFY(!scan.entries[2].id.isEmpty());
    }

    void aFileReachedTwiceCountsOnce()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("games/Sonic.md"), "sonic");
        const LibraryScan scan = scanLibrary({dir.path(), dir.filePath("games"), dir.filePath("games/../games"), dir.path()}, {});
        QCOMPARE(scan.entries.size(), size_t(1));
        QVERIFY(scan.entries[0].duplicates.isEmpty());
        QCOMPARE(scan.duplicateFiles, 0);
    }

    void sameContentIsOneEntryWithTheOtherPathsListed()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("a/Sonic.md"), "same bytes");
        writeFile(dir.filePath("b/Sonic copy.md"), "same bytes");
        writeFile(dir.filePath("b/Other.md"), "different");
        const LibraryScan scan = scanLibrary({dir.path()}, {});
        QCOMPARE(scan.entries.size(), size_t(2));
        QCOMPARE(scan.duplicateFiles, 1);
        QCOMPARE(scan.entries[0].name, QString("Sonic"));
        QCOMPARE(scan.entries[0].duplicates.size(), 1);
        QVERIFY(scan.entries[0].duplicates[0].endsWith("Sonic copy.md"));
    }

    void tracksOfACueSheetAreNotGames()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("Disc/Game.cue"), "FILE \"Game (Track 1).bin\" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\nFILE Track2.bin BINARY\n");
        writeFile(dir.filePath("Disc/Game (Track 1).bin"), discTrack(true));
        writeFile(dir.filePath("Disc/Track2.bin"), "data two");
        writeFile(dir.filePath("Alone.bin"), "a cartridge");
        writeFile(dir.filePath("Elsewhere.iso"), "an image of something else");
        const LibraryScan scan = scanLibrary({dir.path()}, {});
        QCOMPARE(names(scan), QStringList({"Alone", "Game"}));
        QCOMPARE(scan.entries[1].system, System::SegaCD);
    }

    void gamesCarryTheirHeaderTitleAndRegionAndADiscItsProblems()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("Sega CD/Sonic CD.iso"), discTrack(false, "E", "SONIC THE HEDGEHOG-CD"));
        writeFile(dir.filePath("Sega CD/Broken.cue"), "FILE \"Broken.bin\" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\nFILE \"Gone.bin\" BINARY\n  TRACK 02 AUDIO\n    INDEX 01 00:00:00\n");
        writeFile(dir.filePath("Sega CD/Broken.bin"), discTrack(true, "J"));
        const LibraryScan scan = scanLibrary({dir.path()}, {});
        QCOMPARE(names(scan), QStringList({"Broken", "Sonic CD"}));
        QCOMPARE(scan.entries[0].region, QString("Japan"));
        QCOMPARE(scan.entries[0].size, qint64(discTrack(true).size())); // the tracks that exist, not the small cue sheet
        QCOMPARE(scan.entries[0].problem, QString("Missing track file: Gone.bin"));
        QCOMPARE(scan.entries[1].title, QString("SONIC THE HEDGEHOG-CD"));
        QCOMPARE(scan.entries[1].region, QString("Europe"));
        QVERIFY(scan.entries[1].problem.isEmpty());
    }

    void theDiscsOfOneGameAreOneEntryWithTheOthersHangingFromIt()
    {
        QTemporaryDir dir;
        const auto chd = [](const char* tag) { return QByteArray("MComprHD") + QByteArray(1000, 0) + tag; }; // a disc image is told by its first bytes
        writeFile(dir.filePath("Sega CD/Dracula (USA) (Disc 2).chd"), chd("two"));
        writeFile(dir.filePath("Sega CD/Dracula (USA) (Disc 1).chd"), chd("one"));
        writeFile(dir.filePath("Sega CD/Solo (USA) (Disc 1).chd"), chd("solo"));
        writeFile(dir.filePath("Sega CD/Sonic CD.iso"), discTrack(false, "E", "SONIC"));
        const LibraryScan scan = scanLibrary({dir.path()}, {});
        QCOMPARE(names(scan), QStringList({"Dracula (USA) [2 discs]", "Solo (USA) (Disc 1)", "Sonic CD"}));
        const LibraryEntry& dracula = scan.entries[0];
        QVERIFY(dracula.path.endsWith("(Disc 1).chd")); // the first disc is the one that opens
        QCOMPARE(dracula.otherDiscs.size(), 1);
        QVERIFY(dracula.otherDiscs[0].endsWith("(Disc 2).chd"));
        QCOMPARE(dracula.size, qint64(chd("one").size() + chd("two").size())); // the whole game
        QVERIFY(scan.entries[1].otherDiscs.isEmpty()); // one disc is not a set
    }

    void aMissingFolderIsReportedAndTheRestIsScanned()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("Sonic.md"), "sonic");
        writeFile(dir.filePath("file.txt"), "not a folder");
        const LibraryScan scan = scanLibrary({dir.filePath("gone"), dir.filePath("file.txt"), dir.path()}, {});
        QCOMPARE(scan.missingFolders.size(), 2);
        QCOMPARE(scan.entries.size(), size_t(1));
    }

    void aFileThatIsNotAGameIsRememberedAndNotListed()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("empty.zip"), "not really a zip");
        const LibraryScan scan = scanLibrary({dir.path()}, {});
        QVERIFY(scan.entries.empty());
        QCOMPARE(scan.files.size(), 1);
        QCOMPARE(scan.files.first().system, -1);
    }

    void theCacheIsUsedUntilTheFileChanges()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("Sonic.md");
        writeFile(path, "first");
        LibraryScan first = scanLibrary({dir.path()}, {});
        QCOMPARE(first.entries.size(), size_t(1));

        // A poisoned id in the cache comes back as it is while size and time match.
        QMap<QString, LibraryCachedFile> cache = first.files;
        cache.begin().value().id = "cached-id";
        QCOMPARE(scanLibrary({dir.path()}, cache).entries[0].id, QString("cached-id"));

        writeFile(path, "second, longer");
        const LibraryScan changed = scanLibrary({dir.path()}, cache);
        QVERIFY(changed.entries[0].id != "cached-id");
        QVERIFY(changed.entries[0].id != first.entries[0].id);
    }

    void removedGamesLeaveTheNextScan()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("Sonic.md"), "sonic");
        writeFile(dir.filePath("Columns.gg"), "columns");
        const LibraryScan first = scanLibrary({dir.path()}, {});
        QVERIFY(QFile::remove(dir.filePath("Columns.gg")));
        const LibraryScan second = scanLibrary({dir.path()}, first.files);
        QCOMPARE(names(second), QStringList({"Sonic"}));
        QCOMPARE(second.files.size(), 1);
    }

    void aCancelledScanSaysSo()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("Sonic.md"), "sonic");
        const std::atomic<bool> cancel{true};
        QVERIFY(scanLibrary({dir.path()}, {}, &cancel).cancelled);
    }

    void gamesAreNeverChanged()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("Sonic.md");
        writeFile(path, "sonic");
        const QDateTime before = QFileInfo(path).lastModified();
        scanLibrary({dir.path()}, {});
        QCOMPARE(QFileInfo(path).lastModified(), before);
        QCOMPARE(QFileInfo(path).size(), qint64(5));
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files).size(), 1);
    }

    void statisticsAndCacheSurviveARestart()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("config/library.json");
        LibraryData data;
        data.addSession("game-a", 90000, 1000);
        data.addSession("game-a", 30000, 5000);
        data.addSession("game-a", 1000, 2000); // an older session does not move the last played time back
        data.files["C:/x/Sonic.md"] = {"C:/x/Sonic.md", 5, 77, "game-a", 3};
        QString error;
        QVERIFY2(data.save(file, &error), qPrintable(error));

        const LibraryData loaded = LibraryData::load(file, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(loaded.stats["game-a"].playtimeMs, qint64(121000));
        QCOMPARE(loaded.stats["game-a"].lastPlayedMs, qint64(5000));
        QCOMPARE(loaded.files["C:/x/Sonic.md"].id, QString("game-a"));
        QCOMPARE(loaded.files["C:/x/Sonic.md"].system, 3);
    }

    void aSystemChosenByTheUserSurvivesARestart()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("library.json");
        LibraryData data;
        data.systemChoices["game-a"] = static_cast<int>(System::Sega32X);
        data.coreChoices["game-a"] = "picodrive";
        data.regionChoices["game-a"] = static_cast<int>(GameRegionChoice::Europe);
        QVERIFY(data.save(file));
        const LibraryData loaded = LibraryData::load(file);
        QCOMPARE(loaded.regionChoices.value("game-a"), static_cast<int>(GameRegionChoice::Europe));
        QCOMPARE(loaded.coreChoices.value("game-a"), QString("picodrive"));
        QCOMPARE(loaded.systemChoices.value("game-a"), static_cast<int>(System::Sega32X));
        // A value that is no system is dropped.
        QFile bad(file);
        QVERIFY(bad.open(QIODevice::WriteOnly));
        bad.write("{\"version\":1,\"scan_version\":3,\"system_choices\":{\"x\":99,\"y\":2},\"stats\":{},\"files\":[]}");
        bad.close();
        QCOMPARE(LibraryData::load(file).systemChoices.size(), 1);
        bad.open(QIODevice::WriteOnly);
        bad.write("{\"version\":1,\"scan_version\":5,\"region_choices\":{\"x\":7,\"y\":1},\"files\":[]}");
        bad.close();
        QCOMPARE(LibraryData::load(file).regionChoices.size(), 1);
    }

    // What a game forced to a region puts in the core option has to be a value the core takes.
    void theRegionsOfAGameAreValuesTheCoresTake()
    {
        for (const CoreInfo& core : allCores()) {
            const CoreProfile& profile = coreProfileById(core.id);
            const CoreOption* option = profile.userOption(profile.regionKey);
            QVERIFY2(option, qPrintable(core.id));
            for (const std::string& value : profile.regionValues)
                QVERIFY2(isValidCoreOption(*option, value), qPrintable(core.id + " " + QString::fromStdString(value)));
        }
    }

    void anUnreadableFileIsKeptBeforeItIsReplaced()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("library.json");
        writeFile(file, "{ this is not json");
        QString error;
        LibraryData data = LibraryData::load(file, &error);
        QVERIFY(!error.isEmpty());
        QVERIFY(data.stats.isEmpty());
        data.addSession("g", 1, 1);
        QVERIFY(data.save(file, &error));
        QFile bad(file + ".bad");
        QVERIFY(bad.open(QIODevice::ReadOnly));
        QCOMPARE(bad.readAll(), QByteArray("{ this is not json"));
        QCOMPARE(LibraryData::load(file).stats.size(), 1);
    }

    void aMissingFileIsAnEmptyLibraryWithoutError()
    {
        QTemporaryDir dir;
        QString error;
        const LibraryData data = LibraryData::load(dir.filePath("none.json"), &error);
        QVERIFY(error.isEmpty());
        QVERIFY(data.stats.isEmpty() && data.files.isEmpty());
    }
};

QTEST_MAIN(LibraryTests)
#include "LibraryTests.moc"