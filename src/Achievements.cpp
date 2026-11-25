#include "Achievements.h"

#include <QCoreApplication>
#include <QFile>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSysInfo>
#include <QTimer>
#include <QUrl>

#include "AchievementsConsole.h"
#include "AchievementsDisc.h"
#include "LibretroCore.h"
#include "rc_api_request.h"
#include "rc_api_runtime.h"
#include "rc_api_user.h"
#include "rc_consoles.h"
#include "rc_hash.h"

namespace {

// rcheevos' memory callback has nowhere to carry a pointer, and there is one core.
const LibretroCore* g_core = nullptr;

void coreMemory(uint32_t id, rc_libretro_core_memory_info_t* info)
{
    uint8_t* data = nullptr;
    std::size_t size = 0;
    if (g_core && g_core->memoryRegion(id, &data, &size)) {
        info->data = data;
        info->size = size;
    } else {
        info->data = nullptr;
        info->size = 0;
    }
}

}

struct Achievements::Call {
    rc_client_server_callback_t callback;
    void* data;
};

Achievements::Achievements(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_idle(new QTimer(this))
{
    installDiscReader(); // .chd discs as well as .cue and .iso
    m_client = rc_client_create(&Achievements::readMemory, &Achievements::serverCall);
    rc_client_set_userdata(m_client, this);
    rc_client_set_event_handler(m_client, &Achievements::eventHandler);
    rc_client_enable_logging(m_client, RC_CLIENT_LOG_LEVEL_WARN, &Achievements::logMessage);
    rc_client_set_hardcore_enabled(m_client, 0); // not offered (ADR-0028)

    // rcheevos keeps what could not be sent and retries it from here.
    m_idle->setInterval(500);
    connect(m_idle, &QTimer::timeout, this, [this] {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        rc_client_idle(m_client);
    });
    m_idle->start();
}

Achievements::~Achievements()
{
    m_idle->stop();
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    rc_client_destroy(m_client);
    m_client = nullptr;
    if (m_memoryReady)
        rc_libretro_memory_destroy(&m_memory);
    if (g_core == m_core)
        g_core = nullptr;
}

void Achievements::configure(const AchievementsSettings& settings)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_settings = settings;
    rc_client_set_spectator_mode_enabled(m_client, settings.spectator);
    rc_client_set_encore_mode_enabled(m_client, settings.encore);
    rc_client_set_unofficial_enabled(m_client, settings.unofficial);
    rc_client_set_hardcore_enabled(m_client, 0);
}

void Achievements::loginWithPassword(const QString& username, const QString& password)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_lastError.clear();
    m_loginPending = true;
    m_tokenLogin = false;
    rc_client_begin_login_with_password(m_client, username.toUtf8().constData(), password.toUtf8().constData(), &Achievements::loginDone, this);
}

void Achievements::loginWithToken(const QString& username, const QString& token)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_lastError.clear();
    m_loginPending = true;
    m_tokenLogin = true;
    rc_client_begin_login_with_token(m_client, username.toUtf8().constData(), token.toUtf8().constData(), &Achievements::loginDone, this);
}

void Achievements::logout()
{
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_pending.valid = false;
        rc_client_logout(m_client);
    }
    emit accountChanged();
    emit gameChanged();
}

Achievements::User Achievements::user() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    User user;
    const rc_client_user_t* info = rc_client_get_user_info(m_client);
    if (!info)
        return user;
    user.loggedIn = true;
    user.displayName = QString::fromUtf8(info->display_name);
    user.username = QString::fromUtf8(info->username);
    user.token = QString::fromUtf8(info->token);
    user.score = info->score;
    return user;
}

QString Achievements::lastError() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_lastError;
}

void Achievements::loadGame(System system, const QString& path, const QByteArray& data, const LibretroCore& core)
{
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_pending = {true, system, path, data};
        m_core = &core;
        g_core = &core;
        m_coreClause = QString("%1/%2").arg(core.libraryName().toLower().replace(' ', '_'), QString(core.libraryVersion()).replace(' ', '_'));
        if (!rc_client_get_user_info(m_client))
            return; // the sign-in that is under way (or the user, later) decides
    }
    startLoad();
}

void Achievements::startLoad()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_pending.valid || !m_core || !rc_client_get_user_info(m_client))
        return;
    const Pending pending = m_pending;
    m_pending.valid = false;

    const uint32_t console = achievementsConsoleId(pending.system);
    if (!rc_libretro_is_system_allowed(m_core->libraryName().toUtf8().constData(), console)) {
        notify("RetroAchievements does not accept this core for this system");
        return;
    }
    if (m_memoryReady)
        rc_libretro_memory_destroy(&m_memory);
    m_memoryReady = rc_libretro_memory_init(&m_memory, m_core->memoryMap(), &coreMemory, console) != 0;
    if (!m_memoryReady) {
        notify("Achievements are off: the core does not share its memory");
        return;
    }
    const QByteArray path = pending.path.toUtf8();
    rc_client_begin_identify_and_load_game(m_client, console, path.constData(), pending.data.isEmpty() ? nullptr : reinterpret_cast<const uint8_t*>(pending.data.constData()),
                                           std::size_t(pending.data.size()), &Achievements::loadDone, this);
}

void Achievements::unloadGame()
{
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_pending.valid = false;
        rc_client_unload_game(m_client);
        if (m_memoryReady)
            rc_libretro_memory_destroy(&m_memory);
        m_memoryReady = false;
    }
    emit gameChanged();
}

bool Achievements::gameLoaded() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return rc_client_is_game_loaded(m_client) != 0;
}

void Achievements::changeDisc(const QString& path)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!rc_client_is_game_loaded(m_client))
        return;
    const QByteArray file = path.toUtf8();
    rc_client_begin_identify_and_change_media(m_client, file.constData(), nullptr, 0, &Achievements::changeDone, this);
}

QByteArray Achievements::progress() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!rc_client_is_game_loaded(m_client))
        return {};
    const std::size_t size = rc_client_progress_size(m_client);
    if (size == 0)
        return {};
    QByteArray buffer(qsizetype(size), 0);
    if (rc_client_serialize_progress_sized(m_client, reinterpret_cast<uint8_t*>(buffer.data()), size) != RC_OK)
        return {};
    return buffer;
}

bool Achievements::restoreProgress(const QByteArray& progress)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (progress.isEmpty() || !rc_client_is_game_loaded(m_client))
        return false;
    return rc_client_deserialize_progress_sized(m_client, reinterpret_cast<const uint8_t*>(progress.constData()), std::size_t(progress.size())) == RC_OK;
}

void Achievements::reset()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (rc_client_is_game_loaded(m_client))
        rc_client_reset(m_client);
}

void Achievements::frame()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_client && rc_client_is_game_loaded(m_client))
        rc_client_do_frame(m_client);
}

Achievements::GameSummary Achievements::summary() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    GameSummary summary;
    const rc_client_game_t* game = rc_client_get_game_info(m_client);
    if (!game)
        return summary;
    rc_client_user_game_summary_t counts{};
    rc_client_get_user_game_summary(m_client, &counts);
    summary.loaded = true;
    summary.title = QString::fromUtf8(game->title);
    summary.achievements = counts.num_core_achievements;
    summary.unlocked = counts.num_unlocked_achievements;
    summary.points = counts.points_core;
    summary.pointsUnlocked = counts.points_unlocked;
    return summary;
}

QList<Achievements::Entry> Achievements::entries() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    QList<Entry> result;
    if (!rc_client_is_game_loaded(m_client))
        return result;
    rc_client_achievement_list_t* list = rc_client_create_achievement_list(m_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
                                                                           RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
    if (!list)
        return result;
    for (uint32_t b = 0; b < list->num_buckets; ++b) {
        const rc_client_achievement_bucket_t& bucket = list->buckets[b];
        for (uint32_t a = 0; a < bucket.num_achievements; ++a) {
            const rc_client_achievement_t* achievement = bucket.achievements[a];
            const bool unlocked = achievement->unlocked != RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE;
            const char* badge = unlocked ? achievement->badge_url : achievement->badge_locked_url;
            result.append({QString::fromUtf8(achievement->title), QString::fromUtf8(achievement->description), achievement->points, unlocked,
                           badge ? QString::fromUtf8(badge) : QString()});
        }
    }
    rc_client_destroy_achievement_list(list);
    return result;
}

QString Achievements::userAgent() const
{
    char clause[128] = {};
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_client)
            rc_client_get_user_agent_clause(m_client, clause, sizeof(clause));
    }
    const QString version = QCoreApplication::applicationVersion().isEmpty() ? QString("0.1.0") : QCoreApplication::applicationVersion();
    QString agent = QString("Xfusion/%1 (%2)").arg(version, QSysInfo::prettyProductName());
    if (!m_coreClause.isEmpty())
        agent += " " + m_coreClause;
    return agent + " " + QString::fromUtf8(clause);
}

void Achievements::notify(const QString& text)
{
    emit message(text, QImage());
}

void Achievements::notifyWithBadge(const QString& text, const char* badgeName, const char* badgeUrl, uint32_t imageType)
{
    QString url = badgeUrl ? QString::fromUtf8(badgeUrl) : QString();
    if (url.isEmpty() && badgeName && badgeName[0]) {
        // Some servers leave the address out; rcheevos can build it from the name.
        rc_api_fetch_image_request_t image{};
        image.image_name = badgeName;
        image.image_type = imageType;
        rc_api_request_t request{};
        if (rc_api_init_fetch_image_request(&request, &image) == RC_OK)
            url = QString::fromUtf8(request.url);
        rc_api_destroy_request(&request);
    }
    if (url.isEmpty()) {
        notify(text);
        return;
    }
    // Called from the emulation thread too: the network and the cache are on the GUI thread.
    // The notice goes out with the picture, or without it when the picture does not come: the notice matters more.
    QMetaObject::invokeMethod(this, [this, text, url] { fetchImage(url, [this, text](const QImage& image) { emit message(text, image); }); }, Qt::QueuedConnection);
}

void Achievements::requestBadge(const QString& url)
{
    fetchImage(url, [this, url](const QImage& image) { emit badgeReady(url, image); });
}

void Achievements::fetchImage(const QString& url, const std::function<void(const QImage&)>& done)
{
    const auto cached = m_badges.constFind(url);
    if (cached != m_badges.constEnd()) {
        done(*cached);
        return;
    }
    QNetworkRequest request{QUrl(url)};
    request.setTransferTimeout(8000);
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url, done] {
        QImage image;
        if (reply->error() == QNetworkReply::NoError)
            image = QImage::fromData(reply->readAll());
        if (!image.isNull())
            m_badges.insert(url, image);
        done(image); // null when it did not come
        reply->deleteLater();
    });
}

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

// A short chime, played in the background; the sound is part of the program (resources/sounds/achievement.wav).
void Achievements::playUnlockSound() const
{
#ifdef _WIN32
    static const QByteArray wav = [] {
        QFile file(":/sounds/achievement.wav");
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }();
    if (!wav.isEmpty())
        PlaySoundW(reinterpret_cast<LPCWSTR>(wav.constData()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
#endif
}

// -- the network ----------------------------------------------------------------------------------------------------

// rcheevos asks for a request from whichever thread it is on; the reply is handed back on the GUI thread.
void Achievements::serverCall(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callbackData, rc_client_t* client)
{
    auto* self = static_cast<Achievements*>(rc_client_get_userdata(client));
    auto* call = new Call{callback, callbackData};
    const QString url = QString::fromUtf8(request->url);
    const QByteArray body = request->post_data ? QByteArray(request->post_data) : QByteArray();
    const QString contentType = request->content_type ? QString::fromUtf8(request->content_type) : QString();
    QMetaObject::invokeMethod(self, [self, url, body, contentType, call] { self->send(url, body, contentType, call); }, Qt::QueuedConnection);
}

void Achievements::send(const QString& url, const QByteArray& body, const QString& contentType, Call* call)
{
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(30'000);
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    if (!contentType.isEmpty())
        request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    QNetworkReply* reply = body.isEmpty() ? m_network->get(request) : m_network->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, call] {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray answer = reply->readAll();
        const QByteArray error = reply->errorString().toUtf8();
        rc_api_server_response_t response{};
        if (status > 0) {
            response.http_status_code = status;
            response.body = answer.constData();
            response.body_length = std::size_t(answer.size());
        } else { // it never reached the server: the connection, the name lookup, a timeout
            response.http_status_code = RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR;
            response.body = error.constData();
            response.body_length = std::size_t(error.size());
        }
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            if (m_client)
                call->callback(&response, call->data);
        }
        delete call;
        reply->deleteLater();
    });
}

// -- what rcheevos asks of the frontend and tells it ----------------------------------------------------------------------

uint32_t Achievements::readMemory(uint32_t address, uint8_t* buffer, uint32_t numBytes, rc_client_t* client)
{
    auto* self = static_cast<Achievements*>(rc_client_get_userdata(client));
    return self->m_memoryReady ? rc_libretro_memory_read(&self->m_memory, address, buffer, numBytes) : 0;
}

void Achievements::eventHandler(const rc_client_event_t* event, rc_client_t* client)
{
    auto* self = static_cast<Achievements*>(rc_client_get_userdata(client));
    const AchievementsSettings& s = self->m_settings;
    switch (event->type) {
    case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
        if (event->achievement && s.unlockSound)
            self->playUnlockSound();
        if (event->achievement && s.notifications)
            self->notifyWithBadge(QString("Achievement Unlocked: %1").arg(QString::fromUtf8(event->achievement->title)), event->achievement->badge_name,
                                  event->achievement->badge_url, RC_IMAGE_TYPE_ACHIEVEMENT);
        break;
    case RC_CLIENT_EVENT_GAME_COMPLETED:
        if (s.notifications)
            self->notify("Congratulations! All achievements unlocked.");
        break;
    case RC_CLIENT_EVENT_SUBSET_COMPLETED:
        if (event->subset && s.notifications)
            self->notifyWithBadge(QString("Congratulations! All achievements unlocked for %1.").arg(QString::fromUtf8(event->subset->title)),
                                  event->subset->badge_name, event->subset->badge_url, RC_IMAGE_TYPE_GAME);
        break;
    case RC_CLIENT_EVENT_SERVER_ERROR:
        if (event->server_error)
            self->notify(QString("RetroAchievements error: %1").arg(QString::fromUtf8(event->server_error->error_message)));
        break;
    case RC_CLIENT_EVENT_DISCONNECTED:
        self->notify("RetroAchievements: no connection; unlocks will be sent when it returns");
        break;
    case RC_CLIENT_EVENT_RECONNECTED:
        self->notify("RetroAchievements: connection restored");
        break;
    case RC_CLIENT_EVENT_LEADERBOARD_STARTED:
        if (event->leaderboard && s.leaderboardNotifications)
            self->notify(QString("Leaderboard attempt started: %1").arg(QString::fromUtf8(event->leaderboard->title)));
        break;
    case RC_CLIENT_EVENT_LEADERBOARD_FAILED:
        if (event->leaderboard && s.leaderboardNotifications)
            self->notify(QString("Leaderboard attempt failed: %1").arg(QString::fromUtf8(event->leaderboard->title)));
        break;
    case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
        if (event->leaderboard && s.leaderboardNotifications) {
            QString text = QString("Leaderboard submitted: %1").arg(QString::fromUtf8(event->leaderboard->title));
            if (event->leaderboard->tracker_value)
                text += QString(" -- %1").arg(QString::fromUtf8(event->leaderboard->tracker_value));
            self->notify(text);
        }
        break;
    case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW:
    case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE:
        if (event->leaderboard_tracker && s.leaderboardTrackers)
            self->notify(QString::fromUtf8(event->leaderboard_tracker->display));
        break;
    case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW:
    case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE:
        if (event->achievement && s.progressIndicators)
            self->notify(QString("%1: %2").arg(QString::fromUtf8(event->achievement->title), QString::fromUtf8(event->achievement->measured_progress)));
        break;
    case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW:
        if (event->achievement && s.challengeIndicators)
            self->notify(QString("Challenge: %1").arg(QString::fromUtf8(event->achievement->title)));
        break;
    default:
        break; // rich presence, subsets and the rest are not shown
    }
}

void Achievements::logMessage(const char* text, const rc_client_t*)
{
    qWarning("[achievements] %s", text);
}

void Achievements::loginDone(int result, const char* error, rc_client_t*, void* userdata)
{
    auto* self = static_cast<Achievements*>(userdata);
    self->m_lastError = result == RC_OK ? QString() : QString::fromUtf8(error ? error : rc_error_str(result));
    self->m_loginPending = false;
    emit self->accountChanged();
    if (result == RC_OK) {
        self->startLoad();
    } else if (self->m_tokenLogin) {
        // The saved session, tried without the user asking (the dialog says it when the user signs in by hand).
        self->notify(QString("RetroAchievements: could not sign in (%1). Sign in again in Options > Achievements.").arg(self->m_lastError));
    }
}

void Achievements::changeDone(int result, const char* error, rc_client_t*, void* userdata)
{
    auto* self = static_cast<Achievements*>(userdata);
    if (result != RC_OK && result != RC_HARDCORE_DISABLED)
        self->notify(QString("RetroAchievements: %1").arg(QString::fromUtf8(error ? error : rc_error_str(result))));
}

void Achievements::loadDone(int result, const char* error, rc_client_t* client, void* userdata)
{
    auto* self = static_cast<Achievements*>(userdata);
    emit self->gameChanged();
    if (result != RC_OK) {
        if (self->m_settings.notifications)
            self->notify(QString("RetroAchievements: %1").arg(QString::fromUtf8(error ? error : rc_error_str(result))));
        return;
    }
    // The summary a player expects when a game starts.
    rc_client_user_game_summary_t counts{};
    rc_client_get_user_game_summary(client, &counts);
    const rc_client_game_t* game = rc_client_get_game_info(client);
    QString text = game ? QString::fromUtf8(game->title) + "\n" : QString();
    if (counts.num_core_achievements == 0) {
        text += "This game has no achievements.";
    } else {
        text += QString("You have earned %1 of %2 achievements, and %3 of %4 points.")
                    .arg(counts.num_unlocked_achievements).arg(counts.num_core_achievements).arg(counts.points_unlocked).arg(counts.points_core);
        if (rc_client_has_leaderboards(client))
            text += "\nLeaderboards are DISABLED because Hardcore Mode is off.";
    }
    // The same summary a player sees in other frontends, with the badge of the game.
    self->notifyWithBadge(text, game ? game->badge_name : nullptr, game ? game->badge_url : nullptr, RC_IMAGE_TYPE_GAME);
}
