#pragma once

#include <QString>

// The session token of a RetroAchievements account is kept in settings.json, never the password. It is encrypted
// (AES-128 in counter mode, the key made from this machine's id and the user name, stretched) so a copied file does not
// work on another computer or account. This is a barrier against casual copying, not a vault.
QString encryptToken(const QString& username, const QString& token);
// Empty when the data does not decrypt here (another machine or account, or damaged): treat it as "not signed in".
QString decryptToken(const QString& username, const QString& encrypted);
