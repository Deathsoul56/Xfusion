#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

#include "AchievementsToken.h"
#include "AppSettings.h"
#include "DisplayLayout.h"
#include "RewindBuffer.h"
#include "Shortcuts.h"
#include "VideoFilters.h"

class SettingsTests : public QObject {
    Q_OBJECT

private slots:
    void defaultsMatchTheTwin()
    {
        const AppSettings s;
        QCOMPARE(s.aspect, AspectMode::Core);
        QVERIFY(s.maintainAspect && s.stretch && s.smooth && !s.integerScale && !s.muted);
        QCOMPARE(s.volume, AppSettings::kMaxVolume);
        QCOMPARE(s.gain(), 1.0f);
        QVERIFY(s.pauseWhenUnfocused && !s.backgroundGamepad && !s.confirmSaveLoad && !s.saveStateOnClose);
        QVERIFY(s.showIndicators && s.showRomInfo && !s.showFps && !s.showTime && !s.showPressedKeys);
        QCOMPARE(s.sramInterval, 30);
        QCOMPARE(s.osdSize, 24);
        QCOMPARE(s.fastForwardSkip, 9);
        QCOMPARE(s.rewindMegabytes, 10);
        QCOMPARE(s.rewindInterval, 5);
        QCOMPARE(s.speedMethod, SpeedMethod::SoundSync);
        QCOMPARE(s.stateFolder.location, FileLocation::DataFolder);
    }

    void gainFollowsVolumeMuteAndFastForward()
    {
        AppSettings s;
        s.volume = 50;
        s.volumeFastForward = 20;
        QCOMPARE(s.gain(), 0.5f);
        QCOMPARE(s.gain(true), 0.2f);
        s.muted = true;
        QCOMPARE(s.gain(), 0.0f);
        QCOMPARE(s.gain(true), 0.0f);
    }

    void everySettingSurvivesTheFile()
    {
        AppSettings s;
        s.fullscreenOnOpen = s.backgroundGamepad = s.confirmSaveLoad = s.saveStateOnClose = true;
        s.pauseWhenUnfocused = false;
        s.theme = 5;
        s.showFps = s.showTime = s.showPressedKeys = true;
        s.showIndicators = s.showRomInfo = false;
        s.sramInterval = 0;
        s.aspect = AspectMode::Square;
        s.maintainAspect = s.stretch = s.smooth = false;
        s.integerScale = true;
        s.filter = VideoFilter::Scanlines;
        s.messages = MessageMode::None;
        s.osdSize = 40;
        s.osdCorner = OsdCorner::TopRight;
        s.volume = 35;
        s.volumeFastForward = 10;
        s.muted = s.muteDuringFastForward = s.muteDuringFrameAdvance = true;
        s.audioDevice = "Speakers (Test)";
        s.playbackRate = 44100;
        s.bufferMs = 128;
        s.speedMethod = SpeedMethod::TimerFrameskip;
        s.fixedFrameRate = 72.5;
        s.maxFrameSkip = 3;
        s.fastForwardSkip = 4;
        s.fixedFrameSkip = 2;
        s.rewindMegabytes = 64;
        s.rewindInterval = 7;
        s.sramFolder = {FileLocation::RomFolder, {}};
        s.stateFolder = {FileLocation::Custom, "D:/states"};
        s.biosFolder = {FileLocation::Custom, "D:/bios"};
        s.shortcuts.set(ShortcutAction::Mute, 1, InputBinding::joyButton(8));
        QCOMPARE(AppSettings::fromJson(s.toJson()), s);
    }

    void invalidValuesFallBackToDefaultsAndNumbersAreClamped()
    {
        QJsonObject json = AppSettings().toJson();
        QJsonObject display = json.value("display").toObject();
        display.insert("aspect", "sideways");
        display.insert("osd_size", 9999);
        json.insert("display", display);
        QJsonObject sound = json.value("sound").toObject();
        sound.insert("volume", 400);
        sound.insert("buffer_ms", 1);
        json.insert("sound", sound);
        QJsonObject emulation = json.value("emulation").toObject();
        emulation.insert("fast_forward_skip", 100);
        emulation.insert("rewind_interval", 0);
        json.insert("emulation", emulation);
        const AppSettings loaded = AppSettings::fromJson(json);
        QCOMPARE(loaded.aspect, AspectMode::Core);
        QCOMPARE(loaded.osdSize, 256);
        QCOMPARE(loaded.volume, AppSettings::kMaxVolume);
        QCOMPARE(loaded.bufferMs, 16);
        QCOMPARE(loaded.fastForwardSkip, 15);
        QCOMPARE(loaded.rewindInterval, 1);

        QString error;
        QCOMPARE(AppSettings::fromJson({{"version", 99}}, &error), AppSettings());
        QVERIFY(!error.isEmpty());
    }

    void libraryFoldersRoundTripWithoutEmptyOrRepeatedEntries()
    {
        AppSettings s;
        s.libraryFolders = {"C:/roms", "D:/more roms"};
        QCOMPARE(AppSettings::fromJson(s.toJson()).libraryFolders, s.libraryFolders);
        QJsonObject json = s.toJson();
        QJsonObject files = json["files"].toObject();
        files["library_folders"] = QJsonArray{"C:/roms", "", "C:/roms", 5, "E:/x"};
        json["files"] = files;
        QCOMPARE(AppSettings::fromJson(json).libraryFolders, QStringList({"C:/roms", "E:/x"}));
    }

    void thePatchesFolderRoundTrips()
    {
        AppSettings s;
        QCOMPARE(s.patchFolder.location, FileLocation::DataFolder);
        s.patchFolder.location = FileLocation::Custom;
        s.patchFolder.custom = "D:/my patches";
        const AppSettings loaded = AppSettings::fromJson(s.toJson());
        QCOMPARE(loaded.patchFolder.location, FileLocation::Custom);
        QCOMPARE(loaded.patchFolder.custom, QString("D:/my patches"));
        s.patchFolder.location = FileLocation::RomFolder;
        QCOMPARE(AppSettings::fromJson(s.toJson()).patchFolder.location, FileLocation::RomFolder);
    }

    void coreChoicesRoundTripAndUnknownCoresAreDropped()
    {
        AppSettings s;
        s.coreChoices = {{"genesis", "picodrive"}, {"segacd", "genesis_plus_gx"}};
        QCOMPARE(AppSettings::fromJson(s.toJson()).coreChoices, s.coreChoices);
        QJsonObject json = s.toJson();
        QJsonObject emulation = json["emulation"].toObject();
        emulation["cores"] = QJsonObject{{"genesis", "picodrive"}, {"gamegear", "made-up"}, {"32x", 7}};
        json["emulation"] = emulation;
        QCOMPARE(AppSettings::fromJson(json).coreChoices, (QMap<QString, QString>{{"genesis", "picodrive"}}));
    }

    void coreOptionsRoundTripAndOnlyOnesTheCoreOffersAreKept()
    {
        AppSettings s;
        s.coreOptions = {{"genesis_plus_gx", {{"genesis_plus_gx_region_detect", "pal"}, {"genesis_plus_gx_gg_extra", "enabled"}}},
                         {"picodrive", {{"picodrive_region", "Europe"}}}};
        QCOMPARE(AppSettings::fromJson(s.toJson()).coreOptions, s.coreOptions);

        QJsonObject json = s.toJson();
        QJsonObject emulation = json["emulation"].toObject();
        emulation["core_options"] = QJsonObject{{"genesis_plus_gx", QJsonObject{{"genesis_plus_gx_region_detect", "mars"}, // not a value of the option
                                                                                  {"genesis_plus_gx_made_up", "on"},      // not an option
                                                                                  {"genesis_plus_gx_lcd_filter", "enabled"}}},
                                                 {"no-such-core", QJsonObject{{"x", "y"}}}};
        json["emulation"] = emulation;
        const AppSettings loaded = AppSettings::fromJson(json);
        QCOMPARE(loaded.coreOptions.size(), 1);
        QCOMPARE(loaded.coreOptions.value("genesis_plus_gx"), (QMap<QString, QString>{{"genesis_plus_gx_lcd_filter", "enabled"}}));
    }

    void achievementsSettingsRoundTripAndAnAccountNeedsBothHalves()
    {
        AppSettings s;
        s.achievements.enabled = true;
        s.achievements.username = "someone";
        s.achievements.token = "sealed";
        s.achievements.encore = true;
        s.achievements.durationSeconds = 9;
        s.achievements.leaderboardTrackers = false;
        QCOMPARE(AppSettings::fromJson(s.toJson()).achievements, s.achievements);

        // A name without a token (or the other way round) is no account; the duration stays in range.
        QJsonObject json = s.toJson();
        QJsonObject a = json["achievements"].toObject();
        a["token"] = "";
        a["duration_seconds"] = 500;
        json["achievements"] = a;
        const AppSettings loaded = AppSettings::fromJson(json);
        QVERIFY(loaded.achievements.username.isEmpty() && loaded.achievements.token.isEmpty());
        QCOMPARE(loaded.achievements.durationSeconds, 30);
        QVERIFY(!AppSettings().achievements.enabled); // off until the user turns it on
    }

    void aSavedTokenIsNotThePlainTextAndOnlyOpensForItsAccount()
    {
        const QString sealed = encryptToken("someone", "0123456789abcdef");
        QVERIFY(!sealed.isEmpty());
        QVERIFY(!sealed.contains("0123456789abcdef"));
        QCOMPARE(decryptToken("someone", sealed), QString("0123456789abcdef"));
        QVERIFY(decryptToken("another", sealed) != "0123456789abcdef"); // the key depends on the account
        QVERIFY(decryptToken("someone", "not base64 at all !!").isEmpty());
        QVERIFY(decryptToken("someone", QString()).isEmpty());
        // A new random start each time: the same token never seals to the same text.
        QVERIFY(encryptToken("someone", "0123456789abcdef") != sealed);
    }

    void aVersionTwoFileKeepsItsThemeByName()
    {
        // 7 was "AMOLED (Black/Purple)" and 2 "Dark (Gray)" before the list became the one of snes9xrd.
        QCOMPARE(AppSettings::fromJson({{"version", 2}, {"general", QJsonObject{{"theme", 7}}}}).theme, 18);
        QCOMPARE(AppSettings::fromJson({{"version", 2}, {"general", QJsonObject{{"theme", 2}}}}).theme, 8);
        QCOMPARE(AppSettings::fromJson({{"version", 3}, {"general", QJsonObject{{"theme", 7}}}}).theme, 7);
    }

    void aVersionOneFileKeepsItsVideoAndSoundSettings()
    {
        const QJsonObject v1{{"version", 1}, {"aspect", "stretch"}, {"smooth", true}, {"integer_scale", true}, {"volume", 22}, {"muted", true}};
        const AppSettings loaded = AppSettings::fromJson(v1);
        QVERIFY(!loaded.maintainAspect);
        QVERIFY(loaded.smooth && loaded.integerScale && loaded.muted);
        QCOMPARE(loaded.volume, 22);
        QCOMPARE(loaded.shortcuts, ShortcutMap::defaults());
    }

    void saveAndLoadThroughAFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("settings.json");
        QString error;
        QCOMPARE(AppSettings::load(path, &error), AppSettings()); // a missing file is not an error
        QVERIFY(error.isEmpty());

        AppSettings settings;
        settings.volume = 20;
        settings.smooth = false;
        QVERIFY2(settings.save(path, &error), qPrintable(error));
        QCOMPARE(AppSettings::load(path, &error), settings);

        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write("{ not json");
        file.close();
        QCOMPARE(AppSettings::load(path, &error), AppSettings());
        QVERIFY(!error.isEmpty());
    }

    void everyShortcutHasAUniqueIdAndTheSnes9xrdDefaults()
    {
        QSet<QString> keys;
        for (const ShortcutDef& def : allShortcuts()) {
            QVERIFY2(!keys.contains(def.key), qPrintable(def.key));
            keys.insert(def.key);
        }
        QCOMPARE(allShortcuts().size(), size_t(kShortcutCount));
        const ShortcutMap map = ShortcutMap::defaults();
        QCOMPARE(map.bindings(ShortcutAction::OpenRom)[0], InputBinding::key(Qt::Key_O, 0, Qt::ControlModifier));
        QCOMPARE(map.bindings(ShortcutAction::FastForward)[0], InputBinding::key(Qt::Key_Tab, 0));
        QCOMPARE(map.bindings(ShortcutAction::PauseContinue)[0], InputBinding::key(Qt::Key_P, 0));
        QCOMPARE(map.bindings(ShortcutAction::ToggleFullscreen)[0], InputBinding::key(Qt::Key_F11, 0));
        QCOMPARE(map.bindings(ShortcutAction::Screenshot)[0], InputBinding::key(Qt::Key_F12, 0));
        QCOMPARE(map.bindings(ShortcutAction::SaveState)[0], InputBinding::key(Qt::Key_S, 0, Qt::ControlModifier));
        QCOMPARE(map.bindings(ShortcutAction::UndoState)[0], InputBinding::key(Qt::Key_U, 0, Qt::ControlModifier));
        QCOMPARE(map.bindings(ShortcutAction::Rewind)[0], InputBinding::key(Qt::Key_Y, 0));
        QCOMPARE(map.bindings(quickSaveAction(3))[0], InputBinding::key(Qt::Key_F4, 0, Qt::ShiftModifier));
        QCOMPARE(map.bindings(quickLoadAction(9))[0], InputBinding::key(Qt::Key_F10, 0));
        QVERIFY(map.bindings(ShortcutAction::PowerCycle)[0].isNone());
        QVERIFY(shortcutDef(ShortcutAction::FastForward).hold && shortcutDef(ShortcutAction::Rewind).hold);
        QVERIFY(!shortcutDef(ShortcutAction::PauseContinue).hold);
    }

    void defaultShortcutsNeverClashWithEachOther()
    {
        const ShortcutMap map = ShortcutMap::defaults();
        for (const ShortcutDef& def : allShortcuts())
            for (const InputBinding& binding : map.bindings(def.id))
                if (!binding.isNone())
                    QCOMPARE(map.usesOf(binding), 1);
    }

    void shortcutsMatchKeysWithExactModifiers()
    {
        const ShortcutMap map = ShortcutMap::defaults();
        QCOMPARE(map.matchKey(Qt::Key_F1, 0), std::vector<ShortcutAction>{quickLoadAction(0)});
        QCOMPARE(map.matchKey(Qt::Key_F1, Qt::ShiftModifier), std::vector<ShortcutAction>{quickSaveAction(0)});
        QVERIFY(map.matchKey(Qt::Key_F1, Qt::ControlModifier).empty());
        QCOMPARE(map.matchKey(Qt::Key_S, Qt::ControlModifier), std::vector<ShortcutAction>{ShortcutAction::SaveState});
        QVERIFY(map.matchKey(Qt::Key_S, 0).empty());
    }

    void editingShortcutsAndTheResetButtons()
    {
        ShortcutMap map = ShortcutMap::defaults();
        map.set(ShortcutAction::Mute, 0, InputBinding::key(Qt::Key_P, 0)); // same as Pause
        QCOMPARE(map.usesOf(InputBinding::key(Qt::Key_P, 0)), 2);

        map.resetAction(ShortcutAction::Mute);
        QVERIFY(map.bindings(ShortcutAction::Mute)[0].isNone());
        map.clearAction(ShortcutAction::Reset);
        QVERIFY(map.bindings(ShortcutAction::Reset)[0].isNone());
        map.resetAction(ShortcutAction::Reset);
        QCOMPARE(map.bindings(ShortcutAction::Reset)[0], InputBinding::key(Qt::Key_R, 0, Qt::ControlModifier));

        // Reset to Default for one slot writes defaults there and leaves the other slots alone.
        map.clearAll();
        map.set(ShortcutAction::Quit, 2, InputBinding::joyButton(9));
        map.applyDefaultsToSlot(1);
        QCOMPARE(map.bindings(ShortcutAction::PauseContinue)[1], InputBinding::key(Qt::Key_P, 0));
        QVERIFY(map.bindings(ShortcutAction::PauseContinue)[0].isNone());
        QCOMPARE(map.bindings(ShortcutAction::Quit)[2], InputBinding::joyButton(9));
    }

    void shortcutsKeepGamepadAndKeyBindingsThroughTheFile()
    {
        ShortcutMap map = ShortcutMap::defaults();
        map.set(ShortcutAction::FastForward, 1, InputBinding::joyAxis(5, 1));
        map.set(ShortcutAction::Rewind, 2, InputBinding::joyHat(0, InputBinding::kHatUp));
        QJsonObject json = map.toJson();
        QCOMPARE(ShortcutMap::fromJson(json), map);
        json.remove("Quit"); // a missing action keeps its default
        QCOMPARE(ShortcutMap::fromJson(json).bindings(ShortcutAction::Quit)[0], InputBinding::key(Qt::Key_Q, 0, Qt::ControlModifier));
    }

    void coreAspectFitsInsideTheWindowAndIsCentred()
    {
        const QSize frame(320, 224);
        DisplayOptions options;
        QCOMPARE(displayRect({960, 720}, frame, 4.0 / 3.0, options), QRect(0, 0, 960, 720));
        QCOMPARE(displayRect({1600, 720}, frame, 4.0 / 3.0, options), QRect(320, 0, 960, 720));
        QCOMPARE(displayRect({800, 800}, frame, 2.0, options), QRect(0, 200, 800, 400));
    }

    void aspectChoicesAndStretching()
    {
        const QSize frame(320, 224);
        DisplayOptions options;
        options.aspect = AspectMode::FourThree;
        QCOMPARE(displayRect({960, 720}, frame, 2.0, options), QRect(0, 0, 960, 720));
        options.aspect = AspectMode::Square; // 320:224 is 10:7
        QCOMPARE(displayRect({1000, 1000}, frame, 4.0 / 3.0, options), QRect(0, 150, 1000, 700));
        options.maintainAspect = false;
        QCOMPARE(displayRect({1600, 720}, frame, 2.0, options), QRect(0, 0, 1600, 720));
        options.stretch = false; // its own size, centred
        QCOMPARE(displayRect({1000, 700}, frame, 4.0 / 3.0, options), QRect(340, 238, 320, 224));
    }

    void integerScalingUsesWholeMultiplesOfTheFrameHeight()
    {
        const QSize frame(320, 224);
        DisplayOptions options;
        options.integerScale = true;
        // 700 high fits 3 x 224 = 672; the width follows the aspect ratio.
        QCOMPARE(displayRect({1000, 700}, frame, 4.0 / 3.0, options), QRect(52, 14, 896, 672));
        // A window smaller than one frame falls back to the normal fit.
        options.integerScale = false;
        const QRect normal = displayRect({300, 200}, frame, 4.0 / 3.0, options);
        options.integerScale = true;
        QCOMPARE(displayRect({300, 200}, frame, 4.0 / 3.0, options), normal);
    }

    void anEmptyAreaOrFrameGivesAnEmptyRect()
    {
        const DisplayOptions options;
        QVERIFY(displayRect({0, 0}, {320, 224}, 1.33, options).isNull());
        QVERIFY(displayRect({800, 600}, {0, 0}, 1.33, options).isNull());
    }

    void scanlinesDoubleTheHeightAndDarkenEverySecondLine()
    {
        QImage frame(4, 2, QImage::Format_RGB32);
        frame.fill(QColor(200, 100, 50));
        const QImage out = applyVideoFilter(frame, VideoFilter::Scanlines);
        QCOMPARE(out.size(), QSize(4, 4));
        QCOMPARE(QColor(out.pixel(1, 0)), QColor(200, 100, 50));
        QCOMPARE(QColor(out.pixel(1, 1)), QColor(100, 50, 25));
        QCOMPARE(QColor(out.pixel(1, 2)), QColor(200, 100, 50));
        QCOMPARE(applyVideoFilter(frame, VideoFilter::None).size(), frame.size());
        QCOMPARE(applyVideoFilter(frame, VideoFilter::SharpBilinear4x).size(), QSize(16, 8));
    }

    void everyFilterHasAUniqueKeyAndTheSnes9xrdList()
    {
        QSet<QString> keys;
        for (const VideoFilterInfo& info : allVideoFilters()) {
            QVERIFY2(!keys.contains(info.key), info.key);
            keys.insert(info.key);
            QCOMPARE(videoFilterFromKey(info.key, VideoFilter::None), info.id);
            QCOMPARE(videoFilterKey(info.id), QString(info.key));
        }
        QCOMPARE(allVideoFilters().size(), size_t(22));
        QCOMPARE(videoFilterFromKey("nonsense", VideoFilter::HQ3x), VideoFilter::HQ3x);
    }

    void everyFilterScalesAFrameAndKeepsASolidColour()
    {
        QImage frame(32, 24, QImage::Format_RGB16);
        frame.fill(QColor(200, 100, 50));
        const struct { VideoFilter filter; int factor; } scalers[] = {
            {VideoFilter::Simple2x, 2}, {VideoFilter::Simple3x, 3}, {VideoFilter::Simple4x, 4}, {VideoFilter::SuperEagle, 2},
            {VideoFilter::TwoXSaI, 2}, {VideoFilter::Super2xSaI, 2}, {VideoFilter::EPX, 2}, {VideoFilter::HQ2x, 2},
            {VideoFilter::HQ3x, 3}, {VideoFilter::HQ4x, 4}, {VideoFilter::XBRZ2x, 2}, {VideoFilter::XBRZ3x, 3},
            {VideoFilter::XBRZ4x, 4}, {VideoFilter::XBRZ5x, 5}, {VideoFilter::XBRZ6x, 6}, {VideoFilter::SharpBilinear4x, 4},
        };
        for (const auto& scaler : scalers) {
            const QImage out = applyVideoFilter(frame, scaler.filter);
            QCOMPARE(out.size(), QSize(32 * scaler.factor, 24 * scaler.factor));
            const QColor in = QColor(frame.pixel(5, 5)), got = QColor(out.pixel(out.width() / 2, out.height() / 2));
            QVERIFY2(qAbs(in.red() - got.red()) <= 8 && qAbs(in.green() - got.green()) <= 8 && qAbs(in.blue() - got.blue()) <= 8,
                     videoFilterKey(scaler.filter).toUtf8().constData());
        }
    }

    void pixelArtScalersSmoothADiagonalButKeepTheColours()
    {
        QImage frame(16, 16, QImage::Format_RGB32);
        frame.fill(Qt::black);
        for (int i = 0; i < 16; ++i)
            for (int x = i; x < 16; ++x)
                frame.setPixel(x, i, qRgb(255, 255, 255)); // a white triangle: a long diagonal edge
        for (VideoFilter filter : {VideoFilter::EPX, VideoFilter::TwoXSaI, VideoFilter::Super2xSaI, VideoFilter::SuperEagle,
                                   VideoFilter::HQ2x, VideoFilter::XBRZ2x}) {
            const QImage out = applyVideoFilter(frame, filter);
            QCOMPARE(out.size(), QSize(32, 32));
            // Away from the edge nothing changes.
            QCOMPARE(QColor(out.pixel(30, 2)).red() > 240, true);
            QCOMPARE(QColor(out.pixel(2, 30)).red() < 16, true);
        }
    }

    void ntscFiltersWidenTheFrameAndChangeTheColours()
    {
        QImage frame(256, 224, QImage::Format_RGB16);
        frame.fill(QColor(30, 120, 200));
        for (VideoFilter filter : {VideoFilter::NTSCComposite, VideoFilter::NTSCRF, VideoFilter::NTSCSVideo, VideoFilter::NTSCRGB}) {
            const QImage out = applyVideoFilter(frame, filter);
            QVERIFY(out.width() > frame.width() && out.height() == frame.height());
            const QColor middle(out.pixel(out.width() / 2, out.height() / 2));
            // The picture is a flat blue; the filter bleeds it a little but it must not come out black.
            QVERIFY2(middle.blue() > 100 && middle.blue() > middle.red(), videoFilterKey(filter).toUtf8().constData());
        }
    }

    void rewindGoesBackOneSnapshotAtATime()
    {
        RewindBuffer buffer;
        buffer.reset(1024 * 1024);
        QVERIFY(buffer.enabled());
        QByteArray states[5];
        for (int i = 0; i < 5; ++i) {
            states[i] = QByteArray(4000, 'a');
            for (int k = 0; k <= i; ++k)
                states[i][k * 7] = char(i + 1);
            buffer.push(states[i]);
        }
        QCOMPARE(buffer.depth(), 4);
        QByteArray state;
        for (int i = 3; i >= 0; --i) {
            QVERIFY(buffer.pop(&state));
            QCOMPARE(state, states[i]);
        }
        QVERIFY(!buffer.pop(&state)); // history exhausted
    }

    void rewindDifferencesAreSmallAndExact()
    {
        QByteArray a(100000, 0), b(100000, 0);
        b[10] = 5;
        b[50000] = 9;
        b[99999] = 1;
        const QByteArray diff = RewindBuffer::encodeDifference(a, b);
        QVERIFY2(diff.size() < 40, qPrintable(QString::number(diff.size())));
        QCOMPARE(RewindBuffer::applyDifference(a, diff), b);
        QCOMPARE(RewindBuffer::applyDifference(b, diff), a); // XOR is its own inverse
        QCOMPARE(RewindBuffer::applyDifference(a, RewindBuffer::encodeDifference(a, a)), a);
        QVERIFY(RewindBuffer::applyDifference(a, QByteArray("\x05\xFF", 2)).isEmpty()); // truncated: refused
    }

    void rewindDropsTheOldestHistoryWithinItsBudget()
    {
        RewindBuffer buffer;
        buffer.reset(20000);
        QByteArray state(8000, 0);
        for (int i = 0; i < 200; ++i) {
            for (int k = 0; k < 400; ++k)
                state[(i * 37 + k * 11) % state.size()] = char(i + k);
            buffer.push(state);
            QVERIFY(buffer.bytesUsed() <= 20000);
        }
        QVERIFY(buffer.depth() > 0 && buffer.depth() < 199);
        QByteArray out;
        int steps = 0;
        while (buffer.pop(&out))
            ++steps;
        QCOMPARE(steps, 0 + steps); // every kept step could be popped
        QVERIFY(steps > 0);
    }

    void aDifferentStateSizeRestartsTheHistoryAndZeroBudgetDisablesIt()
    {
        RewindBuffer buffer;
        buffer.reset(100000);
        buffer.push(QByteArray(100, 1));
        buffer.push(QByteArray(100, 2));
        QCOMPARE(buffer.depth(), 1);
        buffer.push(QByteArray(200, 3)); // another game
        QCOMPARE(buffer.depth(), 0);

        RewindBuffer off;
        off.reset(0);
        off.push(QByteArray(100, 1));
        off.push(QByteArray(100, 2));
        QVERIFY(!off.enabled());
        QCOMPARE(off.depth(), 0);
    }};

QTEST_APPLESS_MAIN(SettingsTests)
#include "SettingsTests.moc"
