#include <QDir>
#include <QTemporaryDir>
#include <QtTest>
#include <zlib.h>

#include "Patch.h"

namespace {

void writeFile(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(path));
    file.write(bytes);
}

quint32 crc(const QByteArray& bytes)
{
    return quint32(crc32(0, reinterpret_cast<const Bytef*>(bytes.constData()), uInt(bytes.size())));
}

QByteArray le32(quint32 value)
{
    QByteArray bytes;
    for (int i = 0; i < 4; ++i)
        bytes.append(char((value >> (8 * i)) & 0xFF));
    return bytes;
}

QByteArray be(quint32 value, int count)
{
    QByteArray bytes;
    for (int i = count - 1; i >= 0; --i)
        bytes.append(char((value >> (8 * i)) & 0xFF));
    return bytes;
}

// The variable-length number of BPS and UPS.
QByteArray varint(quint64 value)
{
    QByteArray bytes;
    for (;;) {
        const quint8 x = value & 0x7F;
        value >>= 7;
        if (value == 0) {
            bytes.append(char(x | 0x80));
            return bytes;
        }
        bytes.append(char(x));
        --value;
    }
}

// Seals a BPS or UPS patch with the checksums of the game, of the result and of the patch itself.
QByteArray seal(QByteArray body, const QByteArray& source, const QByteArray& target)
{
    body += le32(crc(source)) + le32(crc(target));
    return body + le32(crc(body));
}

const QByteArray kSource = "HELLO WORLD";
const QByteArray kTarget = "HELLO THERE WORLD WORLD";

// HELLO from the game, " THERE" new, " WORLD" copied from the game, then the last six bytes copied again.
QByteArray bpsPatch()
{
    QByteArray body = "BPS1" + varint(kSource.size()) + varint(kTarget.size()) + varint(0);
    body += varint((5 - 1) << 2 | 0); // SourceRead 5
    body += varint((6 - 1) << 2 | 1) + " THERE"; // TargetRead 6
    body += varint((6 - 1) << 2 | 2) + varint(5 << 1); // SourceCopy 6 from offset 5
    body += varint((6 - 1) << 2 | 3) + varint(11 << 1); // TargetCopy 6 from offset 11
    return seal(body, kSource, kTarget);
}

const QByteArray kUpsSource = "ABCDEFGH";
const QByteArray kUpsTarget = "ABXDEFGHIJ";

QByteArray upsPatch()
{
    QByteArray body = "UPS1" + varint(kUpsSource.size()) + varint(kUpsTarget.size());
    body += varint(2) + QByteArray(1, char('C' ^ 'X')) + QByteArray(1, 0); // at 2, then the terminator
    body += varint(4) + QByteArray("IJ") + QByteArray(1, 0); // four bytes on, at 8
    return seal(body, kUpsSource, kUpsTarget);
}

}

class PatchTests : public QObject {
    Q_OBJECT

private slots:
    void theFormatIsToldByTheFirstBytes()
    {
        PatchFormat format;
        QVERIFY(patchFormatOf("PATCHxxxx", &format) && format == PatchFormat::Ips);
        QVERIFY(patchFormatOf("IPS32xxxx", &format) && format == PatchFormat::Ips);
        QVERIFY(patchFormatOf("BPS1xxxx", &format) && format == PatchFormat::Bps);
        QVERIFY(patchFormatOf("UPS1xxxx", &format) && format == PatchFormat::Ups);
        QVERIFY(!patchFormatOf("not a patch", &format));
        QVERIFY(!applyPatch("game", "not a patch").ok);
    }

    void ipsChangesRepeatsExtendsAndTruncates()
    {
        const QByteArray game = "0123456789";
        QByteArray patch = "PATCH";
        patch += be(2, 3) + be(3, 2) + "abc"; // 2..4 = abc
        patch += be(6, 3) + be(0, 2) + be(2, 2) + "z"; // 6..7 = zz (run length encoded)
        patch += be(12, 3) + be(2, 2) + "XY"; // past the end: the game grows, the gap is zeros
        patch += "EOF";
        PatchResult result = applyPatch(game, patch);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(result.data, QByteArray("01abc5zz89") + QByteArray(2, 0) + "XY");

        patch += be(8, 3); // the length the game is cut to
        result = applyPatch(game, patch);
        QVERIFY(result.ok);
        QCOMPARE(result.data, QByteArray("01abc5zz"));
        QCOMPARE(game, QByteArray("0123456789")); // the original is not touched
    }

    void ipsWithoutAnEndMarkOrCutShortIsRefused()
    {
        QVERIFY(!applyPatch("0123", QByteArray("PATCH") + be(0, 3) + be(1, 2) + "a").ok); // no EOF
        const PatchResult cut = applyPatch("0123", QByteArray("PATCH") + be(0, 3) + be(4, 2) + "a");
        QVERIFY(!cut.ok);
        QVERIFY(cut.error.contains("damaged"));
    }

    void ips32UsesFourByteOffsets()
    {
        const PatchResult result = applyPatch("0123456789", QByteArray("IPS32") + be(3, 4) + be(2, 2) + "ab" + "EEOF");
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(result.data, QByteArray("012ab56789"));
    }

    void bpsAppliesAllFourKindsOfChange()
    {
        const PatchResult result = applyPatch(kSource, bpsPatch());
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(result.data, kTarget);
        QCOMPARE(result.format, PatchFormat::Bps);
    }

    void bpsRefusesAnotherGameAndADamagedPatch()
    {
        const PatchResult other = applyPatch("HELLO WORLX", bpsPatch());
        QVERIFY(!other.ok);
        QVERIFY(other.error.contains("different version"));

        QByteArray damaged = bpsPatch();
        damaged[10] = char(damaged[10] ^ 0x55);
        QVERIFY(!applyPatch(kSource, damaged).ok);
        QVERIFY(!applyPatch(kSource, bpsPatch().left(20)).ok);
    }

    void upsPatchesBothWays()
    {
        PatchResult forward = applyPatch(kUpsSource, upsPatch());
        QVERIFY2(forward.ok, qPrintable(forward.error));
        QCOMPARE(forward.data, kUpsTarget);
        PatchResult back = applyPatch(kUpsTarget, upsPatch());
        QVERIFY2(back.ok, qPrintable(back.error));
        QCOMPARE(back.data, kUpsSource);
        QVERIFY(!applyPatch("ABCDEFGX", upsPatch()).ok);
    }

    void aPatchIsFoundByTheNameOfTheGameInTheFolders()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("patches/Genesis/Sonic.ips"), "PATCHEOF");
        writeFile(dir.filePath("patches/Sonic.bps"), "BPS1");
        writeFile(dir.filePath("patches/Other.ips"), "PATCHEOF");
        writeFile(dir.filePath("patches/Zip Game.UPS"), "UPS1");
        const QStringList folders = {dir.filePath("patches/Genesis"), dir.filePath("patches")};
        // The first folder that has one wins; inside a folder .bps comes before .ups and .ips.
        QCOMPARE(findPatch("C:/roms/Sonic.md", folders), QDir(dir.filePath("patches/Genesis")).absoluteFilePath("Sonic.ips"));
        QCOMPARE(findPatch("C:/roms/Sonic.md", {dir.filePath("patches")}), QDir(dir.filePath("patches")).absoluteFilePath("Sonic.bps"));
        QVERIFY(findPatch("C:/roms/Zip Game.zip", folders).endsWith("Zip Game.UPS"));
        QVERIFY(findPatch("C:/roms/Nothing.md", folders).isEmpty());
        QVERIFY(findPatch("C:/roms/Sonic.md", {}).isEmpty());
        QVERIFY(findPatch("C:/roms/Sonic.md", {dir.filePath("missing")}).isEmpty());
    }

    void aGameIsReadFromAPlainFile()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("Game.MD"), "rom bytes");
        QByteArray data;
        QString extension, error;
        QVERIFY(readGameBytes(dir.filePath("Game.MD"), &data, &extension, &error));
        QCOMPARE(data, QByteArray("rom bytes"));
        QCOMPARE(extension, QString("md"));
        QVERIFY(!readGameBytes(dir.filePath("none.md"), &data, &extension, &error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_MAIN(PatchTests)
#include "PatchTests.moc"