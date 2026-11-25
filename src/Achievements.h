#pragma once

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QList>
#include <QObject>
#include <QString>
#include <functional>
#include <memory>
#include <mutex>

#include "AppSettings.h"
#include "SystemInfo.h"
#include "rc_client.h"
#include "rc_libretro.h"

class LibretroCore;
class QNetworkAccessManager;
class QTimer;

// RetroAchievements, in casual mode only (ADR-0028): the account, the achievements of the game that is running and the
// notices. A thin owner of rcheevos' rc_client_t.
//
// Threads: rc_client_t is not thread safe but it has no thread of its own either, so every call into it is made with
// `m_mutex` held, whichever thread it comes from: the emulation thread (`frame()`, once per frame), the GUI thread (the
// account and game calls, the replies of the network and a timer that lets the client retry what failed). The network
// lives on the GUI thread, so a request made from the emulation thread is handed over with a queued call. Notices leave
// as a signal, which Qt queues for the GUI thread when it is another one that emits.
class Achievements : public QObject {
    Q_OBJECT
public:
    struct User {
        bool loggedIn = false;
        QString displayName;
        QString username;
        QString token;
        unsigned score = 0;
    };
    struct GameSummary {
        bool loaded = false;
        QString title;
        unsigned achievements = 0;
        unsigned unlocked = 0;
        unsigned points = 0;
        unsigned pointsUnlocked = 0;
    };
    struct Entry {
        QString title;
        QString description;
        unsigned points = 0;
        bool unlocked = false;
        QString badgeUrl; // the picture of the achievement, in colour when it is unlocked and grey when it is not
    };

    explicit Achievements(QObject* parent = nullptr);
    ~Achievements() override;

    // What the user chose (not the account). Notices and modes; takes effect at once or at the next game, as rcheevos says.
    void configure(const AchievementsSettings& settings);

    void loginWithPassword(const QString& username, const QString& password);
    void loginWithToken(const QString& username, const QString& token);
    void logout();
    bool loginPending() const { return m_loginPending; }
    User user() const;
    QString lastError() const;

    // Identifies the game and loads its achievements, once the account is signed in (right away if it is, or when the
    // sign-in being made ends). `data` is the game itself for a cartridge and empty for a disc, which is read from
    // `path`. `core` supplies the memory the achievements read and says who it is to the server.
    void loadGame(System system, const QString& path, const QByteArray& data, const LibretroCore& core);
    void unloadGame();
    // A game with its achievements is loaded now.
    bool gameLoaded() const;
    // Another disc of the same game was put in the drive: rcheevos is told, which identifies it by its image.
    void changeDisc(const QString& path);
    // Casual mode lets a state load or a reset happen at any time; the achievements start counting again from it.
    void reset();
    // What the achievements have counted so far, to keep with a save state, and putting it back when the state is
    // loaded (false when it does not fit the game that is loaded: the caller then resets).
    QByteArray progress() const;
    bool restoreProgress(const QByteArray& progress);
    // Once per emulated frame, on the emulation thread, after the core ran.
    void frame();

    GameSummary summary() const;
    QList<Entry> entries() const;
    // Fetches a picture (GUI thread); badgeReady() says when it is here, at once if it is in the cache.
    void requestBadge(const QString& url);

signals:
    // `icon` is the badge of the achievement or of the game, or null.
    void message(const QString& text, const QImage& icon);
    void accountChanged(); // signed in or out, or the attempt failed
    void gameChanged(); // a game was loaded or unloaded
    void badgeReady(const QString& url, const QImage& image); // an answer to requestBadge()

private:
    struct Call;
    static uint32_t readMemory(uint32_t address, uint8_t* buffer, uint32_t numBytes, rc_client_t* client);
    static void serverCall(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callbackData, rc_client_t* client);
    static void eventHandler(const rc_client_event_t* event, rc_client_t* client);
    static void logMessage(const char* message, const rc_client_t* client);
    static void loginDone(int result, const char* error, rc_client_t* client, void* userdata);
    static void loadDone(int result, const char* error, rc_client_t* client, void* userdata);
    static void changeDone(int result, const char* error, rc_client_t* client, void* userdata);

    QString userAgent() const;
    void send(const QString& url, const QByteArray& body, const QString& contentType, Call* call);
    void startLoad();
    void notify(const QString& text);
    // A notice with a picture: the badge is fetched (once; later ones come from the cache) and the notice goes out
    // with it, or without it when it cannot be had in time.
    void notifyWithBadge(const QString& text, const char* badgeName, const char* badgeUrl, uint32_t imageType);
    void fetchImage(const QString& url, const std::function<void(const QImage&)>& done);
    void playUnlockSound() const;

    rc_client_t* m_client = nullptr;
    QNetworkAccessManager* m_network;
    QTimer* m_idle;
    mutable std::recursive_mutex m_mutex;
    AchievementsSettings m_settings;
    bool m_loginPending = false;
    bool m_tokenLogin = false; // the sign-in under way uses the saved token
    QString m_lastError;

    // The game waiting for the sign-in, or being loaded.
    struct Pending {
        bool valid = false;
        System system = System::Genesis;
        QString path;
        QByteArray data;
    } m_pending;
    const LibretroCore* m_core = nullptr;
    QString m_coreClause; // "genesis_plus_gx/v1.7.4_4fd32ef"
    QHash<QString, QImage> m_badges; // by address; the GUI thread only
    rc_libretro_memory_regions_t m_memory{};
    bool m_memoryReady = false;
};
