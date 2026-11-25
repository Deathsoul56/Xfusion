// The Schannel backend of Qt Network refers to QPasswordDigestor to open encrypted private keys of client certificates,
// which Xfusion never uses. In Qt's own library those two functions rely on OpenSSL, and linking them in pulls in the
// whole of libcrypto (about 4 MB) for nothing. These are the same two functions written over QMessageAuthenticationCode
// and QCryptographicHash, which are part of Qt Core; being defined here, the ones in Qt Network are not linked in.
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QPasswordDigestor>
#include <QtEndian>

namespace QPasswordDigestor {

QByteArray deriveKeyPbkdf1(QCryptographicHash::Algorithm algorithm, const QByteArray& password, const QByteArray& salt, int iterations, quint64 dkLen)
{
    if (iterations < 1 || dkLen < 1 || salt.size() != 8)
        return {};
    // PBKDF1 (RFC 8018): the hash of password and salt, hashed again for each iteration, cut to the length asked for.
    QByteArray key = QCryptographicHash::hash(password + salt, algorithm);
    if (quint64(key.size()) < dkLen)
        return {};
    for (int i = 1; i < iterations; ++i)
        key = QCryptographicHash::hash(key, algorithm);
    return key.left(qsizetype(dkLen));
}

QByteArray deriveKeyPbkdf2(QCryptographicHash::Algorithm algorithm, const QByteArray& password, const QByteArray& salt, int iterations, quint64 dkLen)
{
    if (iterations < 1 || dkLen < 1)
        return {};
    // PBKDF2 (RFC 8018) with HMAC as the pseudo-random function.
    QByteArray key;
    for (quint32 block = 1; quint64(key.size()) < dkLen; ++block) {
        QByteArray message = salt;
        const quint32 index = qToBigEndian(block);
        message.append(reinterpret_cast<const char*>(&index), sizeof(index));
        QByteArray u = QMessageAuthenticationCode::hash(message, password, algorithm);
        QByteArray t = u;
        for (int i = 1; i < iterations; ++i) {
            u = QMessageAuthenticationCode::hash(u, password, algorithm);
            for (qsizetype j = 0; j < t.size(); ++j)
                t[j] = char(t[j] ^ u[j]);
        }
        key += t;
    }
    return key.left(qsizetype(dkLen));
}

}
