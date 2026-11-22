#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// Downloads cores from the libretro buildbot and installs them next to the ones already there (ADR-0027). Nothing here
// is automatic: it only acts when the Cores dialog asks. One download at a time.
class CoreInstaller : public QObject {
    Q_OBJECT
public:
    explicit CoreInstaller(QObject* parent = nullptr);

    // The buildbot file of the latest build of a core for this system; empty when there is none.
    static QString downloadUrl(const QString& coreId);
    bool busy() const { return m_reply != nullptr; }

    // Asks the server for the date of the latest build, without downloading it.
    void checkForUpdate(const QString& coreId);
    // Downloads the latest build and installs it as the active version.
    void install(const QString& coreId);

signals:
    void progress(const QString& message);
    // `serverDate` is an ISO date of the latest build; empty with an `error`.
    void checked(const QString& coreId, const QString& serverDate, const QString& error);
    // `alreadyThere`: the download was a build that is installed already, so it was only made the active one.
    void finished(const QString& coreId, const QString& version, bool alreadyThere, const QString& error);

private:
    void finishInstall(const QString& coreId, const QByteArray& zip, const QString& serverDate, const QString& source);

    QNetworkAccessManager* m_network;
    QNetworkReply* m_reply = nullptr;
};
