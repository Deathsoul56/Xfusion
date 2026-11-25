#include "AchievementsToken.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <array>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <fstream>
#include <string>
#endif

extern "C" {
#include "rhash/aes.h"
}

namespace {

// Not secret: it only ties the token to this computer. Without it the key is made from the user name alone.
QByteArray machineId()
{
#ifdef _WIN32
    char buffer[64] = {};
    DWORD size = sizeof(buffer);
    if (RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", "MachineGuid", RRF_RT_REG_SZ, nullptr, buffer, &size) == ERROR_SUCCESS)
        return QByteArray(buffer);
    return {};
#else
    std::ifstream file("/etc/machine-id");
    std::string id;
    return file && std::getline(file, id) ? QByteArray::fromStdString(id) : QByteArray();
#endif
}

QByteArray deriveKey(const QString& username)
{
    QByteArray digest = QCryptographicHash::hash(machineId() + username.toUtf8(), QCryptographicHash::Sha256);
    for (int i = 0; i < 100; ++i)
        digest = QCryptographicHash::hash(digest, QCryptographicHash::Sha256);
    return digest;
}

}

QString encryptToken(const QString& username, const QString& token)
{
    if (username.isEmpty() || token.isEmpty())
        return {};
    const QByteArray key = deriveKey(username);
    std::array<uint8_t, AES_BLOCKLEN> iv{};
    for (std::size_t i = 0; i < iv.size(); i += 4) {
        const quint32 word = QRandomGenerator::system()->generate(); // a new one each time, so the stream is never reused
        std::memcpy(&iv[i], &word, 4);
    }
    AES_ctx context;
    AES_init_ctx_iv(&context, reinterpret_cast<const uint8_t*>(key.constData()), iv.data());
    QByteArray text = token.toUtf8();
    AES_CTR_xcrypt_buffer(&context, reinterpret_cast<uint8_t*>(text.data()), size_t(text.size()));
    return QString::fromLatin1((QByteArray(reinterpret_cast<const char*>(iv.data()), int(iv.size())) + text).toBase64());
}

QString decryptToken(const QString& username, const QString& encrypted)
{
    if (username.isEmpty() || encrypted.isEmpty())
        return {};
    const QByteArray payload = QByteArray::fromBase64(encrypted.toLatin1());
    if (payload.size() <= AES_BLOCKLEN)
        return {};
    const QByteArray key = deriveKey(username);
    AES_ctx context;
    AES_init_ctx_iv(&context, reinterpret_cast<const uint8_t*>(key.constData()), reinterpret_cast<const uint8_t*>(payload.constData()));
    QByteArray text = payload.mid(AES_BLOCKLEN);
    AES_CTR_xcrypt_buffer(&context, reinterpret_cast<uint8_t*>(text.data()), size_t(text.size()));
    return QString::fromUtf8(text);
}
