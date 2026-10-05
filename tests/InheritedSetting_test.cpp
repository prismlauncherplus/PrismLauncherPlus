#include <QTemporaryDir>
#include <QTest>

#include <settings/INISettingsObject.h>
#include <settings/InheritableSettings.h>
#include <settings/InheritedSetting.h>

// global settings <- group settings <- instance settings, the way instances and instance groups are set up
class InheritedSettingTest : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    std::unique_ptr<INISettingsObject> m_global;
    std::unique_ptr<INISettingsObject> m_group;
    std::unique_ptr<INISettingsObject> m_instance;
    bool m_inGroup = true;

    std::shared_ptr<Setting> globalSetting(const QString& id)
    {
        if (auto setting = m_global->getSetting(id)) {
            return setting;
        }
        return m_global->registerSetting(id, QString());
    }

   private slots:
    void init()
    {
        m_inGroup = true;
        m_global = std::make_unique<INISettingsObject>(m_dir.filePath("global.cfg"));
        m_global->registerSetting("MaxMemAlloc", 4096);
        m_global->registerSetting("JavaPath", "java");

        m_group = std::make_unique<INISettingsObject>(m_dir.filePath("group.cfg"));
        InheritableSettings::ParentLookup groupParent = [this](const QString& id) { return globalSetting(id); };
        InheritableSettings::registerCommon(m_group.get(), groupParent);
        InheritableSettings::registerMinecraft(m_group.get(), groupParent);

        m_instance = std::make_unique<INISettingsObject>(m_dir.filePath("instance.cfg"));
        InheritableSettings::ParentLookup instanceParent = [this](const QString& id) {
            return std::make_shared<InheritedSetting>(globalSetting(id), [this] { return m_inGroup ? m_group.get() : nullptr; });
        };
        InheritableSettings::registerCommon(m_instance.get(), instanceParent);
        InheritableSettings::registerMinecraft(m_instance.get(), instanceParent);
    }

    void cleanup()
    {
        m_instance.reset();
        m_group.reset();
        m_global.reset();
        for (const auto& file : { "global.cfg", "group.cfg", "instance.cfg" }) {
            QFile::remove(m_dir.filePath(file));
        }
    }

    void test_inheritsGlobalWithoutOverrides() { QCOMPARE(m_instance->get("MaxMemAlloc").toInt(), 4096); }

    void test_groupOverridesGlobal()
    {
        m_group->set("OverrideMemory", true);
        m_group->set("MaxMemAlloc", 8192);
        QCOMPARE(m_instance->get("MaxMemAlloc").toInt(), 8192);
        QCOMPARE(m_global->get("MaxMemAlloc").toInt(), 4096);
    }

    void test_groupValueIgnoredWithoutGate()
    {
        m_group->set("MaxMemAlloc", 8192);
        QCOMPARE(m_instance->get("MaxMemAlloc").toInt(), 4096);
    }

    void test_instanceOverridesGroup()
    {
        m_group->set("OverrideMemory", true);
        m_group->set("MaxMemAlloc", 8192);
        m_instance->set("OverrideMemory", true);
        m_instance->set("MaxMemAlloc", 2048);
        QCOMPARE(m_instance->get("MaxMemAlloc").toInt(), 2048);
        QCOMPARE(m_group->get("MaxMemAlloc").toInt(), 8192);
    }

    void test_followsGroupChanges()
    {
        m_group->set("OverrideJavaLocation", true);
        m_group->set("JavaPath", "/group/java");
        QCOMPARE(m_instance->get("JavaPath").toString(), QString("/group/java"));
        m_inGroup = false;
        QCOMPARE(m_instance->get("JavaPath").toString(), QString("java"));
    }

    void test_overrideEqualToParentIsKept()
    {
        m_group->set("OverrideMemory", true);
        m_group->set("MaxMemAlloc", 8192);
        // override with the value that is inherited anyway
        m_instance->set("OverrideMemory", true);
        m_instance->set("MaxMemAlloc", 8192);
        // the override must not follow the group
        m_group->set("MaxMemAlloc", 4096);
        QCOMPARE(m_instance->get("MaxMemAlloc").toInt(), 8192);
    }

    void test_overrideWithEmptyValueSurvivesReload()
    {
        // global and group have no profile, the instance opts out explicitly
        m_instance->set("OverrideGameOptionsProfile", true);
        m_instance->set("GameOptionsProfile", "");
        m_instance->reload();
        m_group->set("OverrideGameOptionsProfile", true);
        m_group->set("GameOptionsProfile", "group-profile");
        QCOMPARE(m_instance->get("GameOptionsProfile").toString(), QString());
    }

    void test_gameOptionsProfileSelection()
    {
        m_global->set("GameOptionsProfile", "global-profile");
        QCOMPARE(m_instance->get("GameOptionsProfile").toString(), QString("global-profile"));

        m_group->set("OverrideGameOptionsProfile", true);
        m_group->set("GameOptionsProfile", "group-profile");
        QCOMPARE(m_instance->get("GameOptionsProfile").toString(), QString("group-profile"));

        // overriding with no profile opts the instance out of the group's profile
        m_instance->set("OverrideGameOptionsProfile", true);
        m_instance->set("GameOptionsProfile", "");
        QCOMPARE(m_instance->get("GameOptionsProfile").toString(), QString());
    }

    void test_passthroughWritesToTheOverridingLevel()
    {
        // nobody overrides the java location: java info is cached globally
        m_instance->set("JavaVersion", "17");
        QCOMPARE(m_global->get("JavaVersion").toString(), QString("17"));

        // the group overrides it: java info is cached in the group only, the global cache stays valid for the global java
        m_group->set("OverrideJavaLocation", true);
        m_instance->set("JavaVersion", "21");
        QCOMPARE(m_group->get("JavaVersion").toString(), QString("21"));
        QCOMPARE(m_instance->get("JavaVersion").toString(), QString("21"));

        QCOMPARE(m_global->get("JavaVersion").toString(), QString("17"));
    }
};

QTEST_GUILESS_MAIN(InheritedSettingTest)

#include "InheritedSetting_test.moc"
