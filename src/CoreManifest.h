#pragma once

#include <QList>
#include <QMap>
#include <QString>

// The cores installed through Xfusion, side by side: one folder per core and version under cores/, and a manifest that
// says which version of each core is the active one (ADR-0027). A core that sits directly in cores/ (copied by hand
// or by scripts/setup-*.ps1) is the "local file", which plays whenever no version is active.
struct InstalledCore {
    QString version; // what the core says it is: "v1.7.4 4fd32ef"
    QString sha256; // of the library file
    QString path; // of the library file, relative to the cores folder
    QString source; // where it was downloaded from
    QString serverDate; // the date the server gave the download (Last-Modified), to tell whether a newer one is out
    QString installedAt; // ISO date and time
};

class CoreManifest {
public:
    // A missing file is an empty manifest. An unreadable one is too, with `error` set.
    static CoreManifest load(const QString& coresDir, QString* error = nullptr);
    bool save(QString* error = nullptr) const;

    const QString& coresDir() const { return m_coresDir; }
    QList<InstalledCore> versions(const QString& coreId) const { return m_cores.value(coreId).versions; }
    QString activeVersion(const QString& coreId) const { return m_cores.value(coreId).active; }
    const InstalledCore* find(const QString& coreId, const QString& version) const;
    const InstalledCore* active(const QString& coreId) const;
    // The absolute path of the active version's library; empty when none is active.
    QString activePath(const QString& coreId) const;

    // Adds a version (replacing one with the same name) and optionally makes it the active one.
    void add(const QString& coreId, const InstalledCore& core, bool makeActive);
    // False when the version is not installed.
    bool setActive(const QString& coreId, const QString& version);
    // Back to the local file.
    void clearActive(const QString& coreId) { m_cores[coreId].active.clear(); }
    // False when the version is not installed or is the active one.
    bool remove(const QString& coreId, const QString& version);

private:
    struct Entry {
        QString active;
        QList<InstalledCore> versions;
    };
    QString m_coresDir;
    QMap<QString, Entry> m_cores;
};

// The folder name for a version: letters, digits, dots and dashes; anything else becomes "_".
QString versionFolderName(const QString& version);
