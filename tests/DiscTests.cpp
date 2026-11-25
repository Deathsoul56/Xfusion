#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

#include "Bios.h"
#include "CoreRules.h"
#include "DiscInfo.h"
#include "RomInfo.h"
#include "GameHeader.h"
#include "SystemInfo.h"

namespace {

void writeFile(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(path));
    file.write(bytes);
}

QByteArray headerBlock(const QByteArray& system, const QByteArray& domestic, const QByteArray& overseas, const QByteArray& product,
                       const QByteArray& region)
{
    QByteArray block(0x100, ' ');
    block.replace(0, system.size(), system);
    block.replace(0x20, domestic.size(), domestic);
    block.replace(0x50, overseas.size(), overseas);
    block.replace(0x80, product.size(), product);
    block.replace(0xF0, region.size(), region);
    return block;
}

// A Sega CD data track: raw 2352-byte sectors (mode 1) or cooked 2048-byte ones.
QByteArray discTrack(bool raw, const QByteArray& region = "U", const QByteArray& title = "SONIC THE HEDGEHOG-CD")
{
    QByteArray user(0x100, ' ');
    user.replace(0, 14, "SEGADISCSYSTEM");
    user.replace(0x10, 11, "SONICCD    ");
    user += headerBlock("SEGA GENESIS    (C)SEGA 1993.OCT", title, title, "GM MK-4407 -00", region);
    user += QByteArray(2048 - user.size(), ' ');
    if (!raw)
        return user + QByteArray(2048, 'x');
    const QByteArray header("\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x02\x00\x01", 16);
    return header + user + QByteArray(288, 0) + QByteArray(2352, 'x');
}

QByteArray biosImage(const QByteArray& version, const QByteArray& region)
{
    QByteArray image(128 * 1024, 0);
    const QByteArray block = headerBlock("SEGA MEGA DRIVE (C)SEGA 1993.JUN", "CD2 BOOT ROM     06/01-1993     20:00      " + version,
                                         "CD2 BOOT ROM", "BR 000007-2.00", region);
    image.replace(0x100, block.size(), block);
    return image;
}

// A zip with the given files, each stored or deflated. The checksums are left at zero: the reader does not use them.
void writeZip(const QString& path, const std::vector<std::tuple<QString, QByteArray, bool>>& files)
{
    const auto le16 = [](quint16 v) {
        QByteArray bytes;
        bytes.append(char(v & 0xFF));
        bytes.append(char(v >> 8));
        return bytes;
    };
    const auto le32 = [&](quint32 v) { return le16(v & 0xFFFF) + le16(v >> 16); };
    QByteArray body, directory;
    for (const auto& [name, content, deflate] : files) {
        // qCompress gives a zlib stream after a 4-byte length: drop that, the 2-byte header and the 4-byte checksum.
        const QByteArray data = deflate ? qCompress(content, 9).mid(6).chopped(4) : content;
        const quint32 offset = quint32(body.size());
        const QByteArray fileName = name.toUtf8();
        body += QByteArray("PK\x03\x04", 4) + le16(20) + le16(0) + le16(deflate ? 8 : 0) + le32(0) + le32(0) + le32(data.size()) + le32(content.size())
                + le16(fileName.size()) + le16(0) + fileName + data;
        directory += QByteArray("PK\x01\x02", 4) + le16(20) + le16(20) + le16(0) + le16(deflate ? 8 : 0) + le32(0) + le32(0) + le32(data.size())
                     + le32(content.size()) + le16(fileName.size()) + le16(0) + le16(0) + le16(0) + le16(0) + le32(0) + le32(offset) + fileName;
    }
    const QByteArray end = QByteArray("PK\x05\x06", 4) + le16(0) + le16(0) + le16(files.size()) + le16(files.size()) + le32(directory.size())
                           + le32(body.size()) + le16(0);
    writeFile(path, body + directory + end);
}

QByteArray cartridge(qint64 headerAt)
{
    QByteArray rom(0x4000, 0);
    const QByteArray block = headerBlock("SEGA MEGA DRIVE (C)SEGA 1991.APR", "SONIC", "SONIC THE HEDGEHOG", "GM 00001009-00", "JUE");
    rom.replace(int(headerAt), block.size(), block);
    return rom;
}

}

class DiscTests : public QObject {
    Q_OBJECT

private slots:
    void regionFieldsInTheirTwoNotations()
    {
        QCOMPARE(parseRegionField("J"), unsigned(GameHeader::Japan));
        QCOMPARE(parseRegionField("U"), unsigned(GameHeader::Americas));
        QCOMPARE(parseRegionField("E"), unsigned(GameHeader::Europe)); // the letter, not the hexadecimal 14
        QCOMPARE(parseRegionField("JUE"), unsigned(GameHeader::Japan | GameHeader::Americas | GameHeader::Europe));
        QCOMPARE(parseRegionField("4"), unsigned(GameHeader::Americas));
        QCOMPARE(parseRegionField("8"), unsigned(GameHeader::Europe));
        QCOMPARE(parseRegionField("5"), unsigned(GameHeader::Japan | GameHeader::Americas));
        QCOMPARE(parseRegionField("SEGA"), 0u);
        QCOMPARE(parseRegionField("      "), 0u);
        GameHeader header;
        header.regions = GameHeader::Japan | GameHeader::Americas;
        QCOMPARE(header.regionText(), QString("Japan / USA"));
        header.regions = GameHeader::Japan | GameHeader::Americas | GameHeader::Europe;
        QCOMPARE(header.regionText(), QString("World"));
        header.regions = 0;
        QVERIFY(header.regionText().isEmpty());
    }

    void aCookedIsoIsRecognisedByItsFirstSector()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("Sonic CD.iso"), discTrack(false));
        const DiscInfo disc = readDisc(dir.filePath("Sonic CD.iso"));
        QVERIFY(disc.isSegaCd);
        QVERIFY(disc.format.startsWith("ISO"));
        QCOMPARE(disc.volume, QString("SONICCD"));
        QCOMPARE(disc.header.title(), QString("SONIC THE HEDGEHOG-CD"));
        QCOMPARE(disc.header.product, QString("MK-4407 -00"));
        QCOMPARE(disc.header.regions, unsigned(GameHeader::Americas));
        QVERIFY(disc.problems.isEmpty());
        QCOMPARE(systemForFile(dir.filePath("Sonic CD.iso")), std::optional<System>(System::SegaCD));
    }

    void audioFilesBesideAnIsoAreItsTracks()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("Sonic CD.iso"), discTrack(false));
        for (const char* name : {"Sonic CD 02.mp3", "Sonic CD 03.mp3", "sonic cd 10.WAV", "Sonic CD 01.mp3", "Other 02.mp3", "Sonic CD 02.txt"})
            writeFile(dir.filePath(name), "audio");
        const DiscInfo disc = readDisc(dir.filePath("Sonic CD.iso"));
        QCOMPARE(disc.audioTracks, 3);
    }

    void onlyMpeg1At44100HzMp3sArePlayableByPicoDrive()
    {
        QTemporaryDir dir;
        const QByteArray body(3000, 0x55);
        writeFile(dir.filePath("good.mp3"), QByteArray("\xFF\xFB\x90\x00", 4) + body);
        writeFile(dir.filePath("mpeg2.mp3"), QByteArray("\xFF\xF3\x90\x00", 4) + body); // 22.05 kHz
        writeFile(dir.filePath("48k.mp3"), QByteArray("\xFF\xFB\x94\x00", 4) + body); // 48 kHz
        QByteArray tag("ID3\x03\x00\x00\x00\x00\x00\x0A", 10); // a 10 byte tag
        writeFile(dir.filePath("tagged.mp3"), tag + QByteArray(10, 0) + QByteArray("\xFF\xFB\x90\x00", 4) + body);
        writeFile(dir.filePath("noise.mp3"), body);
        QVERIFY(mp3IsPlayableByPicoDrive(dir.filePath("good.mp3")));
        QVERIFY(mp3IsPlayableByPicoDrive(dir.filePath("tagged.mp3")));
        QVERIFY(!mp3IsPlayableByPicoDrive(dir.filePath("mpeg2.mp3")));
        QVERIFY(!mp3IsPlayableByPicoDrive(dir.filePath("48k.mp3")));
        QVERIFY(!mp3IsPlayableByPicoDrive(dir.filePath("noise.mp3")));
        QVERIFY(!mp3IsPlayableByPicoDrive(dir.filePath("missing.mp3")));

        // A disc says which of its tracks are the problem.
        writeFile(dir.filePath("Game.iso"), discTrack(false));
        writeFile(dir.filePath("Game 02.mp3"), QByteArray("\xFF\xFB\x90\x00", 4) + body);
        writeFile(dir.filePath("Game 03.mp3"), QByteArray("\xFF\xF3\x90\x00", 4) + body);
        writeFile(dir.filePath("Game 04.mp3"), QByteArray("\xFF\xF3\x90\x00", 4) + body);
        const DiscInfo disc = readDisc(dir.filePath("Game.iso"));
        QCOMPARE(disc.audioTracks, 3);
        QCOMPARE(disc.unplayableMp3, QStringList({"Game 03.mp3", "Game 04.mp3"}));
    }

    void aRawBinIsADiscTrackAndACartridgeBinIsAGame()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("Disc.bin"), discTrack(true, "J"));
        writeFile(dir.filePath("Cart.bin"), cartridge(0x100));
        const DiscInfo disc = readDisc(dir.filePath("Disc.bin"));
        QVERIFY(disc.isSegaCd);
        QVERIFY(disc.format.contains("2352"));
        QCOMPARE(disc.header.regions, unsigned(GameHeader::Japan));
        QCOMPARE(systemForFile(dir.filePath("Disc.bin")), std::optional<System>(System::SegaCD));
        QVERIFY(!readDisc(dir.filePath("Cart.bin")).isSegaCd);
        QCOMPARE(systemForFile(dir.filePath("Cart.bin")), std::optional<System>(System::Genesis));
    }

    void anIsoOfAnotherConsoleIsNotASegaCdGame()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("Other.iso"), QByteArray(4096, 'x'));
        QVERIFY(!readDisc(dir.filePath("Other.iso")).isSegaCd);
        QVERIFY(!systemForFile(dir.filePath("Other.iso")));
    }

    void aCueSheetCountsItsTracksAndNamesWhatIsMissing()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("Game.cue"),
                  "FILE \"Game (Track 1).bin\" BINARY\r\n  TRACK 01 MODE1/2352\r\n    INDEX 01 00:00:00\r\n"
                  "FILE \"Game (Track 2).bin\" BINARY\r\n  TRACK 02 AUDIO\r\n    INDEX 00 00:00:00\r\n    INDEX 01 00:02:00\r\n"
                  "FILE Game3.bin BINARY\r\n  TRACK 03 AUDIO\r\n    INDEX 01 00:00:00\r\n");
        writeFile(dir.filePath("Game (Track 1).bin"), discTrack(true));
        writeFile(dir.filePath("Game (Track 2).bin"), QByteArray(2352, 0));
        DiscInfo disc = readDisc(dir.filePath("Game.cue"));
        QVERIFY(disc.isSegaCd);
        QCOMPARE(disc.dataTracks, 1);
        QCOMPARE(disc.audioTracks, 2);
        QCOMPARE(disc.problems, QStringList({"Missing track file: Game3.bin"}));
        QCOMPARE(disc.header.title(), QString("SONIC THE HEDGEHOG-CD"));

        QFile::remove(dir.filePath("Game (Track 1).bin"));
        disc = readDisc(dir.filePath("Game.cue"));
        QVERIFY(!disc.isSegaCd);
        QVERIFY(disc.problems.contains("Missing track file: Game (Track 1).bin"));
    }

    void aCueOfAnotherConsoleOrWithoutDataIsNotASegaCdGame()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("Other.cue"), "FILE \"Other.bin\" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n");
        writeFile(dir.filePath("Other.bin"), QByteArray(4096, 'x'));
        QVERIFY(!readDisc(dir.filePath("Other.cue")).isSegaCd);
        writeFile(dir.filePath("Audio.cue"), "FILE \"a.wav\" WAVE\n  TRACK 01 AUDIO\n    INDEX 01 00:00:00\n");
        writeFile(dir.filePath("a.wav"), "x");
        QVERIFY(!readDisc(dir.filePath("Audio.cue")).isSegaCd);
        QVERIFY(!readDisc(dir.filePath("missing.cue")).isSegaCd);
    }

    void aChdIsTakenAsADiscByItsMagic()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("Game.chd"), QByteArray("MComprHD") + QByteArray(100, 0));
        writeFile(dir.filePath("Fake.chd"), "nothing");
        QVERIFY(readDisc(dir.filePath("Game.chd")).isSegaCd);
        QVERIFY(!readDisc(dir.filePath("Fake.chd")).isSegaCd);
    }

    void cartridgeHeadersWithAndWithoutACopierHeader()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("Plain.md"), cartridge(0x100));
        writeFile(dir.filePath("Copier.md"), cartridge(0x300));
        writeFile(dir.filePath("Junk.md"), QByteArray(0x4000, 'x'));
        writeFile(dir.filePath("Zipped.zip"), "PK");
        for (const char* name : {"Plain.md", "Copier.md"}) {
            const GameHeader header = readCartridgeHeader(dir.filePath(name));
            QVERIFY(header.valid);
            QCOMPARE(header.title(), QString("SONIC THE HEDGEHOG"));
            QCOMPARE(header.regionText(), QString("World"));
        }
        QVERIFY(!readCartridgeHeader(dir.filePath("Junk.md")).valid);
        QVERIFY(!readCartridgeHeader(dir.filePath("Zipped.zip")).valid);
    }

    void headersAreReadFromInsideZipsWhetherStoredOrDeflated()
    {
        QTemporaryDir dir;
        const QByteArray rom = cartridge(0x100);
        writeZip(dir.filePath("Deflated.zip"), {{"readme.txt", "hello", true}, {"Sonic.md", rom, true}});
        writeZip(dir.filePath("Stored.zip"), {{"Sonic.bin", rom, false}});
        QByteArray sms(0x8000, 0);
        sms.replace(0x7FF0, 8, "TMR SEGA");
        sms[0x7FFF] = char(0x40);
        writeZip(dir.filePath("Master.zip"), {{"Game.sms", sms, true}});
        writeZip(dir.filePath("Nothing.zip"), {{"notes.txt", "nothing here", true}});
        for (const char* name : {"Deflated.zip", "Stored.zip"}) {
            const GameHeader header = readCartridgeHeader(dir.filePath(name));
            QVERIFY2(header.valid, name);
            QCOMPARE(header.title(), QString("SONIC THE HEDGEHOG"));
        }
        QCOMPARE(readCartridgeHeader(dir.filePath("Master.zip")).regionText(), QString("USA / Europe"));
        QVERIFY(!readCartridgeHeader(dir.filePath("Nothing.zip")).valid);
        QCOMPARE(systemForFile(dir.filePath("Deflated.zip")), std::optional<System>(System::Genesis));
    }

    void masterSystemAndGameGearHeaders()
    {
        QTemporaryDir dir;
        for (const auto& [name, nibble, size] : {std::tuple{"Export.sms", 4, 0x8000}, std::tuple{"Japan.gg", 5, 0x4000}, std::tuple{"Small.sms", 4, 0x2000}}) {
            QByteArray rom(size, 0);
            rom.replace(size - 0x10, 8, "TMR SEGA");
            rom[size - 1] = char(nibble << 4 | 0x0C);
            rom[size - 4] = char(0x34);
            rom[size - 3] = char(0x12);
            writeFile(dir.filePath(name), rom);
        }
        const GameHeader exportHeader = readCartridgeHeader(dir.filePath("Export.sms"));
        QVERIFY(exportHeader.valid);
        QCOMPARE(exportHeader.regionText(), QString("USA / Europe"));
        QCOMPARE(exportHeader.product, QString("1234"));
        QCOMPARE(readCartridgeHeader(dir.filePath("Japan.gg")).regionText(), QString("Japan"));
        QCOMPARE(readCartridgeHeader(dir.filePath("Japan.gg")).systemType, QString("Game Gear"));
        QCOMPARE(readCartridgeHeader(dir.filePath("Small.sms")).regionText(), QString("USA / Europe"));
    }

    void everySystemHasItsDefaultCore()
    {
        for (const SystemDef& def : allSystems())
            QCOMPARE(defaultCoreFor(def.id), def.id == System::Sega32X ? CoreId::PicoDrive : CoreId::GenesisPlusGx);
        // The default of each system is a core that can play it.
        for (const SystemDef& def : allSystems())
            QVERIFY(coreInfo(defaultCoreFor(def.id))->supports(def.id));
        QVERIFY(!coreInfo(CoreId::GenesisPlusGx)->supports(System::Sega32X));
        QCOMPARE(systemKey(System::Sega32X), QString("32x"));
    }

    void theUsersChoiceWinsOnlyWhenTheCoreCanPlayTheSystem()
    {
        const auto all = [](const QString&) { return true; };
        QCOMPARE(chooseCore(System::Genesis, {{"genesis", CoreId::PicoDrive}}, all).id, CoreId::PicoDrive);
        QCOMPARE(chooseCore(System::Sega32X, {{"32x", CoreId::GenesisPlusGx}}, all).id, CoreId::PicoDrive);
        QCOMPARE(chooseCore(System::Genesis, {{"genesis", "no-such-core"}}, all).id, CoreId::GenesisPlusGx);
    }

    void segaCdUsesGenesisPlusGxUnlessTheUserChoosesPicoDrive()
    {
        const auto all = [](const QString&) { return true; };
        QCOMPARE(chooseCore(System::SegaCD, {}, all).id, CoreId::GenesisPlusGx);
        QVERIFY(chooseCore(System::SegaCD, {}, all).note.isEmpty());
        QCOMPARE(chooseCore(System::SegaCD, {{"segacd", CoreId::PicoDrive}}, all).id, CoreId::PicoDrive);
    }

    void theDiscsOfAMultiDiscGameAreFoundByTheirNames()
    {
        QTemporaryDir dir;
        for (const char* name : {"Game (USA) (Disc 1).chd", "Game (USA) (Disc 2).chd", "Game (USA) (Disc 3) (Extra).chd", "Game (USA) (Disc 1).cue",
                                 "Other (USA) (Disc 1).chd", "Solo (USA).chd"})
            writeFile(dir.filePath(name), "x");
        const QStringList expected = {dir.filePath("Game (USA) (Disc 1).chd"), dir.filePath("Game (USA) (Disc 2).chd"),
                                      dir.filePath("Game (USA) (Disc 3) (Extra).chd")};
        // Whichever disc is opened, the set is the same and in disc order; another extension or another game is not in it.
        QCOMPARE(discSetOf(dir.filePath("Game (USA) (Disc 2).chd")), expected);
        QCOMPARE(discSetOf(dir.filePath("Game (USA) (Disc 1).chd")), expected);
        QCOMPARE(discSetOf(dir.filePath("Game (USA) (Disc 3) (Extra).chd")), expected);
        // One disc is not a set.
        QCOMPARE(discSetOf(dir.filePath("Other (USA) (Disc 1).chd")), QStringList{dir.filePath("Other (USA) (Disc 1).chd")});
        QCOMPARE(discSetOf(dir.filePath("Solo (USA).chd")), QStringList{dir.filePath("Solo (USA).chd")});
    }

    // A CHD is binary and cannot be made here without chdman: when XFUSION_TEST_CHD points to the CHD of a Sega CD disc
    // (the files of a collection are not in the repository) it is read like any other disc.
    void aRealChdIsReadLikeAnyOtherDisc()
    {
        const QString path = qEnvironmentVariable("XFUSION_TEST_CHD");
        if (path.isEmpty() || !QFileInfo::exists(path))
            QSKIP("set XFUSION_TEST_CHD to the CHD of a Sega CD disc");
        const DiscInfo disc = readDisc(path);
        qInfo() << "CHD:" << disc.format << disc.volume << disc.header.title() << disc.header.product << disc.header.regionText() << disc.dataTracks << disc.audioTracks;
        QVERIFY(disc.isSegaCd);
        QCOMPARE(disc.format, QString("CHD"));
        QVERIFY(!disc.header.title().isEmpty());
        QVERIFY(disc.dataTracks >= 1);
    }

    void theStartMessageHasTheThreeLinesOfSnes9xrd()
    {
        StartInfo info;
        info.title = "SONIC THE HEDGEHOG";
        info.system = "Genesis";
        info.core = "Genesis Plus GX";
        info.size = sizeInBits(512 * 1024);
        info.sram = "none";
        info.product = "MK-1079 -00";
        info.revision = revisionOfProduct(info.product);
        info.hasCrc = true;
        info.crc = 0xABCD;
        info.checksum = Checksum::Ok;
        QCOMPARE(startMessage(info), QString("\"SONIC THE HEDGEHOG\" (NTSC) version 1.0\nGenesis: Genesis Plus GX: 4Mbits, SRAM: none\n"
                                             "ID: MK-1079 -00, CRC32: 0000abcd, Checksum OK"));
        // A patched game fails its checksum, and the line says why; an unpatched one says it is wrong.
        info.checksum = Checksum::Invalid;
        info.pal = true;
        info.patch = "IPS";
        QVERIFY(startMessage(info).endsWith("Checksum OK") == false && startMessage(info).endsWith("IPS patched"));
        QVERIFY(startMessage(info).startsWith("\"SONIC THE HEDGEHOG\" (PAL)"));
        info.patch.clear();
        QVERIFY(startMessage(info).endsWith("Invalid Checksum"));
        info.checksum = Checksum::None;
        info.product.clear();
        info.revision.clear();
        QCOMPARE(startMessage(info).section('\n', 2), QString("CRC32: 0000abcd"));
        QCOMPARE(revisionOfProduct("MK-4407 -01"), QString("1.1"));
        QCOMPARE(revisionOfProduct("7055"), QString());
        QCOMPARE(sizeInBits(32 * 1024), QString("256Kbits"));
        QCOMPARE(crc32Of("123456789"), quint32(0xCBF43926));
    }

    void aMegaDriveChecksumIsTheSumOfTheWordsAfterTheHeader()
    {
        QByteArray rom(0x300, 0);
        rom.replace(0x100, 4, "SEGA");
        rom[0x200] = char(0x12);
        rom[0x201] = char(0x34);
        rom[0x2FE] = char(0x00);
        rom[0x2FF] = char(0x02);
        QCOMPARE(cartridgeChecksum(rom), Checksum::Invalid); // header holds 0
        rom[0x18E] = char(0x12);
        rom[0x18F] = char(0x36);
        QCOMPARE(cartridgeChecksum(rom), Checksum::Ok);
        QCOMPARE(cartridgeChecksum(QByteArray(0x300, 0)), Checksum::None); // not a Mega Drive image
    }

    void aCoreChosenForAGameWinsOverTheOneChosenForItsSystem()
    {
        const auto all = [](const QString&) { return true; };
        const QMap<QString, QString> pico = {{"genesis", CoreId::PicoDrive}};
        QCOMPARE(chooseCore(System::Genesis, pico, all, CoreId::GenesisPlusGx).id, CoreId::GenesisPlusGx);
        QCOMPARE(chooseCore(System::Genesis, {}, all, CoreId::PicoDrive).id, CoreId::PicoDrive);
        QCOMPARE(chooseCore(System::Genesis, pico, all, QString()).id, CoreId::PicoDrive);
        // A core that cannot play the system is ignored, as is one that does not exist.
        QCOMPARE(chooseCore(System::Sega32X, {}, all, CoreId::GenesisPlusGx).id, CoreId::PicoDrive);
        QCOMPARE(chooseCore(System::Genesis, {}, all, "no-such-core").id, CoreId::GenesisPlusGx);
    }

    void aCoreThatIsNotInstalledIsReplacedWhenAnotherCanPlayTheSystem()
    {
        const auto onlyPico = [](const QString& id) { return id == CoreId::PicoDrive; };
        const CoreDecision genesis = chooseCore(System::Genesis, {}, onlyPico);
        QCOMPARE(genesis.id, CoreId::PicoDrive);
        QVERIFY(genesis.note.contains("Genesis Plus GX is not installed"));
        const auto onlyGpgx = [](const QString& id) { return id == CoreId::GenesisPlusGx; };
        QVERIFY(chooseCore(System::Sega32X, {}, onlyGpgx).id.isEmpty()); // nothing else plays 32X
        QVERIFY(chooseCore(System::Genesis, {}, [](const QString&) { return false; }).id.isEmpty());
    }

    void theHeaderCorrectsTheExtensionOfACartridge()
    {
        QTemporaryDir dir;
        QByteArray thirtyTwoX = cartridge(0x100);
        thirtyTwoX.replace(0x100, 8, "SEGA 32X");
        writeFile(dir.filePath("Game.bin"), thirtyTwoX);
        writeFile(dir.filePath("Game.md"), thirtyTwoX);
        writeFile(dir.filePath("Real.md"), cartridge(0x100));
        writeFile(dir.filePath("Game.32x"), cartridge(0x100)); // some 32X games carry a plain Mega Drive header
        writeZip(dir.filePath("Zipped.zip"), {{"Game.bin", thirtyTwoX, true}});
        for (const char* name : {"Game.bin", "Game.md", "Zipped.zip", "Game.32x"})
            QCOMPARE(systemForFile(dir.filePath(name)), std::optional<System>(System::Sega32X));
        QCOMPARE(systemForFile(dir.filePath("Real.md")), std::optional<System>(System::Genesis));

        // A Game Gear game called .sms and a Master System game called .gg.
        for (const auto& [name, nibble] : {std::pair{"Gg.sms", 6}, std::pair{"Sms.gg", 4}}) {
            QByteArray rom(0x8000, 0);
            rom.replace(0x7FF0, 8, "TMR SEGA");
            rom[0x7FFF] = char(nibble << 4);
            writeFile(dir.filePath(name), rom);
        }
        QCOMPARE(systemForFile(dir.filePath("Gg.sms")), std::optional<System>(System::GameGear));
        QCOMPARE(systemForFile(dir.filePath("Sms.gg")), std::optional<System>(System::MasterSystem));
        // Without a header the extension decides, as it must for SG-1000 games.
        writeFile(dir.filePath("Plain.sg"), QByteArray(0x8000, 0));
        writeFile(dir.filePath("Plain.gg"), QByteArray(0x8000, 0));
        QCOMPARE(systemForFile(dir.filePath("Plain.sg")), std::optional<System>(System::SG1000));
        QCOMPARE(systemForFile(dir.filePath("Plain.gg")), std::optional<System>(System::GameGear));
    }

    void aBiosIsRecognisedByItsContentNotItsName()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("Sega Mega-CD Model 2 BIOS v2.00w (1993)(Sega)(Eu)[b].bin"), biosImage("2.00W", "E 2"));
        writeFile(dir.filePath("whatever.bin"), biosImage("2.11X", "U 2"));
        writeFile(dir.filePath("japan.rom"), biosImage("1.00S", "J"));
        writeFile(dir.filePath("short.bin"), QByteArray(1000, 0));
        writeFile(dir.filePath("cart.bin"), cartridge(0x100) + QByteArray(128 * 1024 - 0x4000, 0));
        const auto europe = identifyBios(dir.filePath("Sega Mega-CD Model 2 BIOS v2.00w (1993)(Sega)(Eu)[b].bin"));
        QVERIFY(europe);
        QCOMPARE(europe->region, BiosRegion::Europe);
        QCOMPARE(europe->version, QString("2.00W"));
        QCOMPARE(identifyBios(dir.filePath("whatever.bin"))->region, BiosRegion::USA);
        QCOMPARE(identifyBios(dir.filePath("japan.rom"))->region, BiosRegion::Japan);
        QVERIFY(!identifyBios(dir.filePath("short.bin")));
        QVERIFY(!identifyBios(dir.filePath("cart.bin")));
        QVERIFY(!identifyBios(dir.filePath("nothing.bin")));
    }

    void installingCopiesOnlyWhatIsMissingAndKeepsWhatIsThere()
    {
        QTemporaryDir dir;
        const QString source = dir.filePath("dumps"), system = dir.filePath("system");
        writeFile(source + "/eu.bin", biosImage("2.00W", "E"));
        writeFile(source + "/us-old.bin", biosImage("1.10", "U"));
        writeFile(source + "/us-new.bin", biosImage("2.11X", "U"));
        writeFile(source + "/jp.bin", biosImage("1.00S", "J"));
        writeFile(source + "/readme.txt", "hello");
        writeFile(system + "/bios_CD_J.bin", biosImage("1.00S", "J")); // already there and right
        writeFile(system + "/bios_CD_E.bin", "not a bios");

        QString error;
        const QStringList installed = installBios(source, system, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(installed.size(), 2);
        const BiosStatus status = checkBios(system);
        QVERIFY(status.japan && status.europe && status.usa);
        QCOMPARE(identifyBios(system + "/bios_CD_U.bin")->version, QString("2.11X"));
        QVERIFY(QFileInfo::exists(system + "/bios_CD_E.bin.old")); // the file that was in the way is kept
        QVERIFY(installBios(source, system).isEmpty()); // nothing left to do
    }

    void aBiosWithTheWrongRegionForItsNameDoesNotCount()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("bios_CD_U.bin"), biosImage("2.00W", "E"));
        QVERIFY(!checkBios(dir.path()).usa);
        QVERIFY(!checkBios(dir.path()).europe);
    }
};

QTEST_MAIN(DiscTests)
#include "DiscTests.moc"