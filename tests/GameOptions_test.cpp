#include <QTemporaryDir>
#include <QTest>

#include <minecraft/gameoptions/GameOptionsCompat.h>
#include <minecraft/gameoptions/GameOptionsMerger.h>
#include <minecraft/gameoptions/GameOptionsProfile.h>
#include <minecraft/gameoptions/GameOptionsProfileList.h>
#include <minecraft/gameoptions/OptionsFile.h>

using namespace GameOptionsCompat;

namespace {
// options.txt as written by 1.12.2 (keybinds as LWJGL 2 key codes) and 1.21 (keybinds as key names)
const QByteArray legacyOptions =
    "version:1343\n"
    "fov:0.0\n"
    "renderDistance:12\n"
    "lastServer:mc.example.com:25565\n"
    "key_key.forward:17\n"
    "key_key.attack:-100\n";
const QByteArray modernOptions =
    "version:3955\n"
    "fov:0.5\n"
    "renderDistance:16\n"
    "resourcePacks:[\"vanilla\"]\n"
    "key_key.forward:key.keyboard.w\n"
    "key_key.attack:key.mouse.left\n";
}  // namespace

class GameOptionsTest : public QObject {
    Q_OBJECT

   private slots:
    // OptionsFile

    void test_roundTripKeepsEverything()
    {
        const QByteArray data =
            "version:3955\n"
            "this line is malformed\n"
            "\n"
            "lastServer:mc.example.com:25565\n"
            "unknownModOption:some value\n";
        QCOMPARE(OptionsFile::parse(data).serialize(), data);
    }

    void test_splitsOnFirstColon()
    {
        auto file = OptionsFile::parse("lastServer:mc.example.com:25565\n");
        QCOMPARE(file.value("lastServer"), QString("mc.example.com:25565"));
    }

    void test_crlf()
    {
        auto file = OptionsFile::parse("fov:0.5\r\nrenderDistance:8\r\n");
        QCOMPARE(file.value("fov"), QString("0.5"));
        QCOMPARE(file.value("renderDistance"), QString("8"));
    }

    void test_repeatedKeyUsesLastValue()
    {
        auto file = OptionsFile::parse("fov:0.1\nfov:0.2\n");
        QCOMPARE(file.value("fov"), QString("0.2"));
        file.set("fov", "0.3");
        QCOMPARE(file.serialize(), QByteArray("fov:0.1\nfov:0.3\n"));
        file.remove("fov");
        QVERIFY(!file.contains("fov"));
        QCOMPARE(file.serialize(), QByteArray());
    }

    void test_setAppendsNewKeys()
    {
        auto file = OptionsFile::parse("fov:0.1\n");
        file.set("gamma", "1.0");
        QCOMPARE(file.serialize(), QByteArray("fov:0.1\ngamma:1.0\n"));
    }

    void test_dataVersion()
    {
        QCOMPARE(OptionsFile::parse(modernOptions).dataVersion(), 3955);
        QCOMPARE(OptionsFile::parse("fov:0.0\n").dataVersion(), std::nullopt);
        QCOMPARE(OptionsFile::parse("version:abc\n").dataVersion(), std::nullopt);
    }

    void test_saveAndLoad()
    {
        QTemporaryDir dir;
        auto path = dir.filePath("options.txt");
        QVERIFY(OptionsFile::load(path).has_value());
        QVERIFY(OptionsFile::load(path)->isEmpty());
        QVERIFY(OptionsFile::parse(modernOptions).save(path).has_value());
        QCOMPARE(OptionsFile::load(path)->serialize(), modernOptions);
    }

    // GameOptionsCompat

    void test_instanceSpecificOptionsAreNotShared()
    {
        for (const auto& key :
             { "version", "resourcePacks", "incompatibleResourcePacks", "lastServer", "tutorialStep", "startedCleanly" }) {
            QVERIFY2(!isShareable(key), key);
        }
        QVERIFY(isShareable("fov"));
        QVERIFY(isShareable("key_key.forward"));
    }

    void test_detectKeybindFormatFromValues()
    {
        // the values win over the data version, they were written by the client itself
        QCOMPARE(ClientFormat::detect(OptionsFile::parse("version:9999\nkey_key.jump:57\n")).keybinds, KeybindFormat::Codes);
        QCOMPARE(ClientFormat::detect(OptionsFile::parse(modernOptions)).keybinds, KeybindFormat::Names);
    }

    void test_detectKeybindFormatFromDataVersion()
    {
        QCOMPARE(ClientFormat::detect(OptionsFile::parse("version:1343\n")).keybinds, KeybindFormat::Codes);
        QCOMPARE(ClientFormat::detect(OptionsFile::parse("version:3955\n")).keybinds, KeybindFormat::Names);
        auto fallback = ClientFormat::detect(OptionsFile(), 3955);
        QCOMPARE(fallback.dataVersion, 3955);
        QCOMPARE(fallback.keybinds, KeybindFormat::Names);
        QCOMPARE(ClientFormat::detect(OptionsFile()).keybinds, KeybindFormat::Unknown);
    }

    // GameOptionsProfile

    void test_importSkipsInstanceSpecificOptions()
    {
        auto file = OptionsFile::parse(modernOptions);
        GameOptionsProfile profile;
        profile.importFrom(file, ClientFormat::detect(file), QDateTime::currentDateTimeUtc());
        QVERIFY(profile.options.contains("fov"));
        QVERIFY(!profile.options.contains("version"));
        QVERIFY(!profile.options.contains("resourcePacks"));
        QCOMPARE(profile.value("fov", "")->dataVersion, 3955);
    }

    void test_keybindsAreKeptPerFormat()
    {
        auto legacy = OptionsFile::parse(legacyOptions);
        auto modern = OptionsFile::parse(modernOptions);
        GameOptionsProfile profile;
        const auto now = QDateTime::currentDateTimeUtc();
        profile.importFrom(legacy, ClientFormat::detect(legacy), now);
        profile.importFrom(modern, ClientFormat::detect(modern), now);

        // shared options: the later import wins
        QCOMPARE(profile.options["fov"].size(), 1);
        QCOMPARE(profile.value("fov", "")->value, QString("0.5"));
        // keybinds: both formats are kept
        QCOMPARE(profile.options["key_key.forward"].size(), 2);
    }

    void test_jsonRoundTrip()
    {
        auto legacy = OptionsFile::parse(legacyOptions);
        auto modern = OptionsFile::parse(modernOptions);
        GameOptionsProfile profile;
        profile.id = "some-id";
        profile.name = "Default";
        profile.targetVersion = "1.21";
        const auto now = QDateTime::currentDateTimeUtc();
        profile.importFrom(legacy, ClientFormat::detect(legacy), now);
        profile.importFrom(modern, ClientFormat::detect(modern), now);
        profile.setValue("noDataVersion", "1", std::nullopt, now);

        auto loaded = GameOptionsProfile::fromJson(profile.toJson());
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->id, profile.id);
        QCOMPARE(loaded->name, profile.name);
        QCOMPARE(loaded->targetVersion, profile.targetVersion);
        QCOMPARE(loaded->options.keys(), profile.options.keys());
        QCOMPARE(loaded->options["key_key.forward"].size(), 2);
        QCOMPARE(loaded->value("fov", "")->updated, now);
        QCOMPARE(loaded->value("noDataVersion", "")->dataVersion, std::nullopt);
    }

    void test_jsonRejectsUnknownFormat()
    {
        QVERIFY(!GameOptionsProfile::fromJson(QJsonObject{ { "formatVersion", 99 }, { "id", "x" } }).has_value());
        QVERIFY(!GameOptionsProfile::fromJson(QJsonObject{ { "formatVersion", 1 } }).has_value());
    }

    // GameOptionsMerger

    void test_applyToNewerClient()
    {
        auto legacy = OptionsFile::parse(legacyOptions);
        GameOptionsProfile profile;
        profile.importFrom(legacy, ClientFormat::detect(legacy), QDateTime::currentDateTimeUtc());

        auto modern = OptionsFile::parse(modernOptions);
        auto result = GameOptionsMerger::apply(profile, modern, ClientFormat::detect(modern));

        QCOMPARE(result.file.value("renderDistance"), QString("12"));
        // the 1.12 keybinds can't be read by 1.21, its own stay
        QCOMPARE(result.file.value("key_key.forward"), QString("key.keyboard.w"));
        QVERIFY(result.skipped.contains("key_key.forward"));
        // instance specific options aren't touched
        QCOMPARE(result.file.value("version"), QString("3955"));
        QCOMPARE(result.file.value("resourcePacks"), QString("[\"vanilla\"]"));
        QVERIFY(!result.file.contains("lastServer"));
    }

    void test_applyToOlderClient()
    {
        auto modern = OptionsFile::parse(modernOptions);
        GameOptionsProfile profile;
        profile.importFrom(modern, ClientFormat::detect(modern), QDateTime::currentDateTimeUtc());

        auto legacy = OptionsFile::parse(legacyOptions);
        auto result = GameOptionsMerger::apply(profile, legacy, ClientFormat::detect(legacy));
        QCOMPARE(result.file.value("fov"), QString("0.5"));
        QCOMPARE(result.file.value("key_key.forward"), QString("17"));
        QCOMPARE(result.file.value("version"), QString("1343"));
        QCOMPARE(result.file.value("lastServer"), QString("mc.example.com:25565"));
    }

    void test_applyToNewFile()
    {
        auto modern = OptionsFile::parse(modernOptions);
        GameOptionsProfile profile;
        profile.importFrom(modern, ClientFormat::detect(modern), QDateTime::currentDateTimeUtc());

        // a new file needs a version, or Minecraft treats the values as written by an ancient version
        auto result = GameOptionsMerger::apply(profile, OptionsFile(), ClientFormat::detect(OptionsFile(), 4000));
        QCOMPARE(result.file.keys().first(), QString("version"));
        QCOMPARE(result.file.value("version"), QString("4000"));
        QCOMPARE(result.file.value("key_key.forward"), QString("key.keyboard.w"));

        // without any idea of the version, version dependent options are left out
        auto unknown = GameOptionsMerger::apply(profile, OptionsFile(), ClientFormat::detect(OptionsFile()));
        QVERIFY(!unknown.file.contains("version"));
        QVERIFY(!unknown.file.contains("key_key.forward"));
        QCOMPARE(unknown.file.value("fov"), QString("0.5"));
    }

    void test_collectChanges()
    {
        auto modern = OptionsFile::parse(modernOptions);
        auto snapshot = GameOptionsMerger::snapshotOf(modern);

        auto after = modern;
        after.set("fov", "0.75");                  // changed
        after.set("gamma", "1.0");                 // added
        after.set("resourcePacks", "[]");          // not shareable
        after.set("lastServer", "other.example");  // not shareable
        after.remove("renderDistance");            // removed, ignored

        auto changes = GameOptionsMerger::collectChanges(snapshot, after);
        QCOMPARE(changes.size(), 2);
        QCOMPARE(changes[0].key, QString("fov"));
        QCOMPARE(changes[0].oldValue, QString("0.5"));
        QCOMPARE(changes[0].newValue, QString("0.75"));
        QCOMPARE(changes[1].key, QString("gamma"));
        QCOMPARE(changes[1].oldValue, std::nullopt);
    }

    void test_writeBackOnlyTouchesTheClientsBand()
    {
        auto legacy = OptionsFile::parse(legacyOptions);
        auto modern = OptionsFile::parse(modernOptions);
        GameOptionsProfile profile;
        const auto now = QDateTime::currentDateTimeUtc();
        profile.importFrom(legacy, ClientFormat::detect(legacy), now);

        // a 1.21 instance runs with the profile and rebinds forward
        auto format = ClientFormat::detect(modern);
        auto applied = GameOptionsMerger::apply(profile, modern, format);
        auto after = applied.file;
        after.set("key_key.forward", "key.keyboard.up");
        profile.applyChanges(GameOptionsMerger::collectChanges(applied.snapshot, after), format, now);

        QCOMPARE(profile.value("key_key.forward", bandOf("key_key.forward", "key.keyboard.up"))->value, QString("key.keyboard.up"));
        // the 1.12 binding is still there
        QCOMPARE(profile.value("key_key.forward", bandOf("key_key.forward", "17"))->value, QString("17"));
    }

    // GameOptionsProfileList

    void test_profileListPersists()
    {
        QTemporaryDir dir;
        QString id;
        {
            GameOptionsProfileList list(dir.path());
            list.load();
            auto created = list.createProfile("Survival", "1.21");
            QVERIFY(created.has_value());
            id = *created;
            QVERIFY(list.createProfile("Another").has_value());
            QVERIFY(list.setProfileInfo(id, "Building", "1.20.1").has_value());
        }
        GameOptionsProfileList list(dir.path());
        list.load();
        QCOMPARE(list.rowCount(), 2);
        // sorted by name
        QCOMPARE(list.data(list.index(0)).toString(), QString("Another"));
        QCOMPARE(list.profile(id)->name, QString("Building"));
        QCOMPARE(list.profile(id)->targetVersion, QString("1.20.1"));

        QVERIFY(list.removeProfile(id).has_value());
        QCOMPARE(list.profile(id), nullptr);
        GameOptionsProfileList reloaded(dir.path());
        reloaded.load();
        QCOMPARE(reloaded.rowCount(), 1);
    }

    void test_duplicateCopiesOptions()
    {
        QTemporaryDir dir;
        GameOptionsProfileList list(dir.path());
        auto id = *list.createProfile("Original");
        auto modern = OptionsFile::parse(modernOptions);
        QVERIFY(list.modifyProfile(id,
                                   [&modern](GameOptionsProfile& profile) {
                                       profile.importFrom(modern, ClientFormat::detect(modern), QDateTime::currentDateTimeUtc());
                                   })
                    .has_value());

        auto copy = list.duplicateProfile(id, "Copy");
        QVERIFY(copy.has_value());
        QVERIFY(*copy != id);
        QCOMPARE(list.profile(*copy)->options.keys(), list.profile(id)->options.keys());
    }

    void test_simultaneousWriteBacksKeepBothChanges()
    {
        // two launchers (or two instances with stale copies) writing to the same profile
        QTemporaryDir dir;
        GameOptionsProfileList first(dir.path());
        auto id = *first.createProfile("Shared");
        GameOptionsProfileList second(dir.path());
        second.load();

        auto format = ClientFormat::detect(OptionsFile::parse(modernOptions));
        QVERIFY(first.applyChanges(id, { { "fov", std::nullopt, "0.9" } }, format).has_value());
        // second still has the profile as it was before the first write
        QVERIFY(second.applyChanges(id, { { "gamma", std::nullopt, "1.0" } }, format).has_value());

        GameOptionsProfileList reloaded(dir.path());
        reloaded.load();
        QCOMPARE(reloaded.profile(id)->value("fov", "")->value, QString("0.9"));
        QCOMPARE(reloaded.profile(id)->value("gamma", "")->value, QString("1.0"));
    }

    void test_ignoresBrokenFiles()
    {
        QTemporaryDir dir;
        QFile broken(dir.filePath("broken.json"));
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("{ not json");
        broken.close();
        GameOptionsProfileList list(dir.path());
        list.load();
        QCOMPARE(list.rowCount(), 0);
    }
};

QTEST_GUILESS_MAIN(GameOptionsTest)

#include "GameOptions_test.moc"
