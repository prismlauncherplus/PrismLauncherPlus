// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2024 TheKodeToad <TheKodeToad@proton.me>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "MinecraftSettingsWidget.h"
#include "modplatform/ModIndex.h"
#include "ui_MinecraftSettingsWidget.h"

#include <QFileDialog>
#include "Application.h"
#include "BuildConfig.h"
#include "InstanceList.h"
#include "Json.h"
#include "minecraft/PackProfile.h"
#include "minecraft/WorldList.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/gameoptions/GameOptionsProfileList.h"
#include "settings/Setting.h"

MinecraftSettingsWidget::MinecraftSettingsWidget(MinecraftInstance* instance, SettingsObject* groupSettings, QWidget* parent)
    : QWidget(parent), m_instance(instance), m_groupSettings(groupSettings), m_ui(new Ui::MinecraftSettingsWidget)
{
    m_ui->setupUi(this);

    if (!isOverrideMode()) {
        m_ui->settingsTabs->removeTab(1);

        m_ui->openGlobalSettingsButton->setVisible(false);
        m_ui->instanceAccountGroupBox->hide();
        m_ui->serverJoinGroupBox->hide();
        m_ui->globalDataPacksGroupBox->hide();
        m_ui->loaderGroup->hide();
        m_ui->countGameTime->hide();
        m_ui->latestMCVersionGroupBox->hide();
    } else {
        m_javaSettings = new JavaSettingsWidget(m_instance, m_groupSettings, this);
        m_ui->javaScrollArea->setWidget(m_javaSettings);

        if (m_instance != nullptr) {
            m_ui->showGameTime->setText(tr("Show time &playing this instance"));
            m_ui->recordGameTime->setText(tr("&Record time playing this instance"));
        } else {
            m_ui->showGameTime->setText(tr("Show time &playing instances of this group"));
            m_ui->recordGameTime->setText(tr("&Record time playing instances of this group"));
        }
        m_ui->showGlobalGameTime->hide();
        m_ui->showGameTimeWithoutDays->hide();

        m_ui->maximizedWarning->setText(
            tr("<span style=\" font-weight:600; color:#f5c211;\">Warning</span><span style=\" color:#f5c211;\">: The maximized option is "
               "not fully supported on this Minecraft version.</span>"));

        m_ui->consoleSettingsBox->setCheckable(true);
        m_ui->windowSizeGroupBox->setCheckable(true);
        m_ui->nativeWorkaroundsGroupBox->setCheckable(true);
        m_ui->perfomanceGroupBox->setCheckable(true);
        m_ui->gameTimeGroupBox->setCheckable(true);
        m_ui->legacySettingsGroupBox->setCheckable(true);

        connect(m_ui->openGlobalSettingsButton, &QCommandLinkButton::clicked, this, &MinecraftSettingsWidget::openGlobalSettings);
    }

    if (m_instance == nullptr && m_groupSettings != nullptr) {
        // these settings only make sense for a single instance
        m_ui->instanceAccountGroupBox->hide();
        m_ui->serverJoinGroupBox->hide();
        m_ui->globalDataPacksGroupBox->hide();
        m_ui->loaderGroup->hide();
        m_ui->countGameTime->hide();
        m_ui->latestMCVersionGroupBox->hide();
    }

    if (m_instance != nullptr) {
        m_quickPlaySingleplayer = m_instance->traits().contains("feature:is_quick_play_singleplayer");
        if (m_quickPlaySingleplayer) {
            auto* worlds = m_instance->worldList();
            worlds->update();
            for (const auto& world : worlds->allWorlds()) {
                m_ui->worldsCb->addItem(world.folderName());
            }
        } else {
            m_ui->worldsCb->hide();
            m_ui->worldJoinButton->hide();
            m_ui->serverJoinAddressButton->setChecked(true);
            m_ui->serverJoinAddress->setEnabled(true);
            m_ui->serverJoinAddressButton->setStyleSheet("QRadioButton::indicator { width: 0px; height: 0px; }");
        }

        connect(m_ui->serverJoinAddressButton, &QAbstractButton::toggled, m_ui->serverJoinAddress, &QWidget::setEnabled);
        connect(m_ui->worldJoinButton, &QAbstractButton::toggled, m_ui->worldsCb, &QWidget::setEnabled);

        connect(m_ui->globalDataPacksGroupBox, &QGroupBox::toggled, this, [this](bool value) {
            m_instance->settings()->set("GlobalDataPacksEnabled", value);
            if (!value) {
                m_instance->settings()->reset("GlobalDataPacksPath");
            }
        });
        connect(m_ui->dataPacksPathEdit, &QLineEdit::editingFinished, this, &MinecraftSettingsWidget::saveDataPacksPath);
        connect(m_ui->dataPacksPathBrowse, &QPushButton::clicked, this, &MinecraftSettingsWidget::selectDataPacksFolder);

        connect(m_ui->loaderGroup, &QGroupBox::toggled, this, [this](bool value) {
            m_instance->settings()->set("OverrideModDownloadLoaders", value);
            if (value) {
                saveSelectedLoaders();
            } else {
                m_instance->settings()->reset("ModDownloadLoaders");
            }
        });

        for (auto* c : { m_ui->neoForge, m_ui->forge, m_ui->fabric, m_ui->quilt, m_ui->liteLoader, m_ui->babric, m_ui->btaBabric,
                         m_ui->legacyFabric, m_ui->ornithe, m_ui->rift }) {
            connect(c, &QCheckBox::checkStateChanged, this, &MinecraftSettingsWidget::saveSelectedLoaders);
        }
        auto latestVersion = m_instance->settings()->getSetting("UseLatestMinecraftVersion");
        connect(latestVersion.get(), &Setting::SettingChanged, this, [this](const Setting&, const QVariant&) {
            m_ui->latestMCVersionGroupBox->setChecked(m_instance->settings()->get("UseLatestMinecraftVersion").toBool());
        });
    }

    // Shared game options
    {
        m_ui->gameOptionsGroupBox->setCheckable(isOverrideMode());
        // in the global settings the profiles page is right next to this one
        m_ui->manageGameOptionsButton->setVisible(isOverrideMode());
        connect(m_ui->manageGameOptionsButton, &QPushButton::clicked, this,
                [this] { APPLICATION->ShowGlobalSettings(this, "game-options"); });
        connect(m_ui->gameOptionsProfileComboBox, &QComboBox::currentIndexChanged, this, &MinecraftSettingsWidget::updateGameOptionsInfo);
        connect(m_ui->gameOptionsGroupBox, &QGroupBox::toggled, this, [this](bool overriding) {
            if (overriding) {
                if (m_gameOptionsOverride) {
                    populateGameOptionsProfiles(*m_gameOptionsOverride);
                }
            } else {
                m_gameOptionsOverride = m_ui->gameOptionsProfileComboBox->currentData().toString();
                // show what is used instead
                populateGameOptionsProfiles(inheritedGameOptionsProfile().first);
            }
            updateGameOptionsInfo();
        });

        // keep the dropdown up to date when profiles are added, renamed or removed
        auto* profiles = APPLICATION->gameOptionsProfiles();
        auto repopulate = [this] { populateGameOptionsProfiles(m_ui->gameOptionsProfileComboBox->currentData().toString()); };
        connect(profiles, &QAbstractItemModel::modelReset, this, repopulate);
        connect(profiles, &QAbstractItemModel::rowsRemoved, this, repopulate);
        connect(profiles, &QAbstractItemModel::layoutChanged, this, repopulate);
        connect(profiles, &QAbstractItemModel::dataChanged, this, repopulate);
    }

    m_ui->maximizedWarning->hide();

    connect(m_ui->maximizedCheckBox, &QCheckBox::toggled, this,
            [this](const bool value) { m_ui->maximizedWarning->setVisible(value && (m_instance == nullptr || !m_instance->isLegacy())); });

#if !defined(Q_OS_LINUX)
    m_ui->perfomanceGroupBox->hide();
#endif

    if (!(APPLICATION->capabilities() & Application::SupportsGameMode)) {
        m_ui->enableFeralGamemodeCheck->setDisabled(true);
        m_ui->enableFeralGamemodeCheck->setToolTip(tr("Feral Interactive's GameMode could not be found on your system."));
    }

    if (!(APPLICATION->capabilities() & Application::SupportsMangoHud)) {
        m_ui->enableMangoHud->setEnabled(false);
        m_ui->enableMangoHud->setToolTip(tr("MangoHud could not be found on your system."));
    }

    connect(m_ui->useNativeOpenALCheck, &QAbstractButton::toggled, m_ui->lineEditOpenALPath, &QWidget::setEnabled);
    connect(m_ui->useNativeGLFWCheck, &QAbstractButton::toggled, m_ui->lineEditGLFWPath, &QWidget::setEnabled);
    connect(m_ui->useNativeSDLCheck, &QAbstractButton::toggled, m_ui->lineEditSDLPath, &QWidget::setEnabled);

    loadSettings();
}

MinecraftSettingsWidget::~MinecraftSettingsWidget()
{
    delete m_ui;
}

SettingsObject* MinecraftSettingsWidget::settings() const
{
    if (m_instance != nullptr) {
        return m_instance->settings();
    }
    if (m_groupSettings != nullptr) {
        return m_groupSettings;
    }
    return APPLICATION->settings();
}

void MinecraftSettingsWidget::loadSettings()
{
    SettingsObject* settings = this->settings();

    // Game Window
    m_ui->windowSizeGroupBox->setChecked(!isOverrideMode() || settings->get("OverrideWindow").toBool() ||
                                         settings->get("OverrideMiscellaneous").toBool());
    m_ui->maximizedCheckBox->setChecked(settings->get("LaunchMaximized").toBool());
    m_ui->windowWidthSpinBox->setValue(settings->get("MinecraftWinWidth").toInt());
    m_ui->windowHeightSpinBox->setValue(settings->get("MinecraftWinHeight").toInt());
    m_ui->closeAfterLaunchCheck->setChecked(settings->get("CloseAfterLaunch").toBool());
    m_ui->quitAfterGameStopCheck->setChecked(settings->get("QuitAfterGameStop").toBool());

    // Game Time
    m_ui->gameTimeGroupBox->setChecked(!isOverrideMode() || settings->get("OverrideGameTime").toBool());
    m_ui->showGameTime->setChecked(settings->get("ShowGameTime").toBool());
    m_ui->recordGameTime->setChecked(settings->get("RecordGameTime").toBool());
    m_ui->countGameTime->setChecked(settings->get("CountGameTime").toBool());
    m_ui->showGlobalGameTime->setChecked(!isOverrideMode() && settings->get("ShowGlobalGameTime").toBool());
    m_ui->showGameTimeWithoutDays->setChecked(!isOverrideMode() && settings->get("ShowGameTimeWithoutDays").toBool());

    // Console
    m_ui->consoleSettingsBox->setChecked(!isOverrideMode() || settings->get("OverrideConsole").toBool());
    m_ui->showConsoleCheck->setChecked(settings->get("ShowConsole").toBool());
    m_ui->autoCloseConsoleCheck->setChecked(settings->get("AutoCloseConsole").toBool());
    m_ui->showConsoleErrorCheck->setChecked(settings->get("ShowConsoleOnError").toBool());

    if (m_javaSettings != nullptr) {
        m_javaSettings->loadSettings();
    }

    // Custom commands
    m_ui->customCommands->initialize(isOverrideMode(), !isOverrideMode() || settings->get("OverrideCommands").toBool(),
                                     settings->get("PreLoadCommand").toString(), settings->get("PreLaunchCommand").toString(),
                                     settings->get("WrapperCommand").toString(), settings->get("PostExitCommand").toString());

    // Environment variables
    m_ui->environmentVariables->initialize(isOverrideMode(), !isOverrideMode() || settings->get("OverrideEnv").toBool(),
                                           Json::toMap(settings->get("Env").toString()));

    // Legacy Tweaks
    m_ui->legacySettingsGroupBox->setChecked(!isOverrideMode() || settings->get("OverrideLegacySettings").toBool());
    m_ui->onlineFixes->setChecked(settings->get("OnlineFixes").toBool());

    // Native Libraries
    m_ui->nativeWorkaroundsGroupBox->setChecked(!isOverrideMode() || settings->get("OverrideNativeWorkarounds").toBool());
    m_ui->useNativeGLFWCheck->setChecked(settings->get("UseNativeGLFW").toBool());
    m_ui->lineEditGLFWPath->setText(settings->get("CustomGLFWPath").toString().trimmed());
#ifdef Q_OS_LINUX
    m_ui->lineEditGLFWPath->setPlaceholderText(APPLICATION->m_detectedGLFWPath);
#else
    m_ui->lineEditGLFWPath->setPlaceholderText(tr("Path to %1 library file").arg(BuildConfig.GLFW_LIBRARY_NAME));
#endif
    m_ui->useNativeOpenALCheck->setChecked(settings->get("UseNativeOpenAL").toBool());
    m_ui->lineEditOpenALPath->setText(settings->get("CustomOpenALPath").toString().trimmed());
#ifdef Q_OS_LINUX
    m_ui->lineEditOpenALPath->setPlaceholderText(APPLICATION->m_detectedOpenALPath);
#else
    m_ui->lineEditOpenALPath->setPlaceholderText(tr("Path to %1 library file").arg(BuildConfig.OPENAL_LIBRARY_NAME));
#endif
    m_ui->useNativeSDLCheck->setChecked(settings->get("UseNativeSDL").toBool());
    m_ui->lineEditSDLPath->setText(settings->get("CustomSDLPath").toString().trimmed());
#ifdef Q_OS_LINUX
    m_ui->lineEditSDLPath->setPlaceholderText(APPLICATION->m_detectedSDLPath);
#else
    m_ui->lineEditSDLPath->setPlaceholderText(tr("Path to %1 library file").arg(BuildConfig.SDL_LIBRARY_NAME));
#endif

    // Shared game options, the dropdown shows the inherited profile when not overriding
    m_ui->gameOptionsGroupBox->blockSignals(true);
    m_ui->gameOptionsGroupBox->setChecked(!isOverrideMode() || settings->get("OverrideGameOptionsProfile").toBool());
    m_ui->gameOptionsGroupBox->blockSignals(false);
    populateGameOptionsProfiles(settings->get("GameOptionsProfile").toString());
    m_ui->gameOptionsReviewCheck->setChecked(settings->get("GameOptionsReviewChanges").toBool());

    // Performance
    m_ui->perfomanceGroupBox->setChecked(!isOverrideMode() || settings->get("OverridePerformance").toBool());
    m_ui->enableFeralGamemodeCheck->setChecked(settings->get("EnableFeralGamemode").toBool());
    m_ui->enableMangoHud->setChecked(settings->get("EnableMangoHud").toBool());
    m_ui->useDiscreteGpuCheck->setChecked(settings->get("UseDiscreteGpu").toBool());
    m_ui->useZink->setChecked(settings->get("UseZink").toBool());

    if (m_instance != nullptr) {
        // HACK: if we change enable state of child widgets while it's unchecked this creates inconsistency
        m_ui->serverJoinGroupBox->setChecked(true);

        if (auto server = settings->get("JoinServerOnLaunchAddress").toString(); !server.isEmpty()) {
            m_ui->serverJoinAddress->setText(server);
            m_ui->serverJoinAddressButton->setChecked(true);
            m_ui->worldJoinButton->setChecked(false);
            m_ui->serverJoinAddress->setEnabled(true);
            m_ui->worldsCb->setEnabled(false);
        } else if (auto world = settings->get("JoinWorldOnLaunch").toString(); !world.isEmpty() && m_quickPlaySingleplayer) {
            m_ui->worldsCb->setCurrentText(world);
            m_ui->serverJoinAddressButton->setChecked(false);
            m_ui->worldJoinButton->setChecked(true);
            m_ui->serverJoinAddress->setEnabled(false);
            m_ui->worldsCb->setEnabled(true);
        } else {
            m_ui->serverJoinAddressButton->setChecked(true);
            m_ui->worldJoinButton->setChecked(false);
            m_ui->serverJoinAddress->setEnabled(true);
            m_ui->worldsCb->setEnabled(false);
        }

        m_ui->serverJoinGroupBox->setChecked(settings->get("JoinServerOnLaunch").toBool());

        m_ui->instanceAccountGroupBox->setChecked(settings->get("UseAccountForInstance").toBool());
        updateAccountsMenu(*settings);

        auto blockSignalsCheckBoxes = { m_ui->neoForge, m_ui->forge,     m_ui->fabric,       m_ui->quilt,   m_ui->liteLoader,
                                        m_ui->babric,   m_ui->btaBabric, m_ui->legacyFabric, m_ui->ornithe, m_ui->rift };
        m_ui->loaderGroup->blockSignals(true);
        for (auto* c : blockSignalsCheckBoxes) {
            c->blockSignals(true);
        }

        const bool overrideLoaders = settings->get("OverrideModDownloadLoaders").toBool();
        const QStringList loaders = Json::toStringList(settings->get("ModDownloadLoaders").toString());

        m_ui->loaderGroup->setChecked(overrideLoaders);

        if (overrideLoaders) {
            m_ui->neoForge->setChecked(loaders.contains(getModLoaderAsString(ModPlatform::NeoForge)));
            m_ui->forge->setChecked(loaders.contains(getModLoaderAsString(ModPlatform::Forge)));
            m_ui->fabric->setChecked(loaders.contains(getModLoaderAsString(ModPlatform::Fabric)));
            m_ui->quilt->setChecked(loaders.contains(getModLoaderAsString(ModPlatform::Quilt)));
            m_ui->liteLoader->setChecked(loaders.contains(getModLoaderAsString(ModPlatform::LiteLoader)));
            m_ui->babric->setChecked(loaders.contains(getModLoaderAsString(ModPlatform::Babric)));
            m_ui->btaBabric->setChecked(loaders.contains(getModLoaderAsString(ModPlatform::BTA)));
            m_ui->legacyFabric->setChecked(loaders.contains(getModLoaderAsString(ModPlatform::LegacyFabric)));
            m_ui->ornithe->setChecked(loaders.contains(getModLoaderAsString(ModPlatform::Ornithe)));
            m_ui->rift->setChecked(loaders.contains(getModLoaderAsString(ModPlatform::Rift)));
        } else {
            auto instLoaders = m_instance->getPackProfile()->getSupportedModLoaders().value_or(ModPlatform::ModLoaderTypes(0));

            m_ui->neoForge->setChecked(instLoaders.testAnyFlags(ModPlatform::NeoForge));
            m_ui->forge->setChecked(instLoaders.testAnyFlags(ModPlatform::Forge));
            m_ui->fabric->setChecked(instLoaders.testAnyFlags(ModPlatform::Fabric));
            m_ui->quilt->setChecked(instLoaders.testAnyFlags(ModPlatform::Quilt));
            m_ui->liteLoader->setChecked(instLoaders.testAnyFlags(ModPlatform::LiteLoader));
            m_ui->babric->setChecked(instLoaders.testAnyFlags(ModPlatform::Babric));
            m_ui->btaBabric->setChecked(instLoaders.testAnyFlags(ModPlatform::BTA));
            m_ui->legacyFabric->setChecked(instLoaders.testAnyFlags(ModPlatform::LegacyFabric));
            m_ui->ornithe->setChecked(instLoaders.testAnyFlags(ModPlatform::Ornithe));
            m_ui->rift->setChecked(instLoaders.testAnyFlags(ModPlatform::Rift));
        }

        m_ui->loaderGroup->blockSignals(false);
        for (auto* c : blockSignalsCheckBoxes) {
            c->blockSignals(false);
        }

        m_ui->latestMCVersionGroupBox->setChecked(settings->get("UseLatestMinecraftVersion").toBool());
        auto autoUpdateType = settings->get("UseLatestMinecraftVersionType").toString() == "release";
        m_ui->releaseRadioButton->setChecked(autoUpdateType);
        m_ui->anyRadioButton->setChecked(!autoUpdateType);
    }

    m_ui->legacySettingsGroupBox->setChecked(settings->get("OverrideLegacySettings").toBool());
    m_ui->onlineFixes->setChecked(settings->get("OnlineFixes").toBool());

    m_ui->globalDataPacksGroupBox->blockSignals(true);
    m_ui->dataPacksPathEdit->blockSignals(true);
    m_ui->globalDataPacksGroupBox->setChecked(settings->get("GlobalDataPacksEnabled").toBool());
    m_ui->dataPacksPathEdit->setText(settings->get("GlobalDataPacksPath").toString().trimmed());
    m_ui->globalDataPacksGroupBox->blockSignals(false);
    m_ui->dataPacksPathEdit->blockSignals(false);
}

void MinecraftSettingsWidget::saveSettings()
{
    SettingsObject* settings = this->settings();

    // Console
    bool console = !isOverrideMode() || m_ui->consoleSettingsBox->isChecked();

    if (isOverrideMode()) {
        settings->set("OverrideConsole", console);
    }

    if (console) {
        settings->set("ShowConsole", m_ui->showConsoleCheck->isChecked());
        settings->set("AutoCloseConsole", m_ui->autoCloseConsoleCheck->isChecked());
        settings->set("ShowConsoleOnError", m_ui->showConsoleErrorCheck->isChecked());
    } else {
        settings->reset("ShowConsole");
        settings->reset("AutoCloseConsole");
        settings->reset("ShowConsoleOnError");
    }

    // Game Window
    bool window = !isOverrideMode() || m_ui->windowSizeGroupBox->isChecked();

    if (isOverrideMode()) {
        settings->set("OverrideWindow", window);
        settings->set("OverrideMiscellaneous", window);
    }

    if (window) {
        settings->set("LaunchMaximized", m_ui->maximizedCheckBox->isChecked());
        settings->set("MinecraftWinWidth", m_ui->windowWidthSpinBox->value());
        settings->set("MinecraftWinHeight", m_ui->windowHeightSpinBox->value());
        settings->set("CloseAfterLaunch", m_ui->closeAfterLaunchCheck->isChecked());
        settings->set("QuitAfterGameStop", m_ui->quitAfterGameStopCheck->isChecked());
    } else {
        settings->reset("LaunchMaximized");
        settings->reset("MinecraftWinWidth");
        settings->reset("MinecraftWinHeight");
        settings->reset("CloseAfterLaunch");
        settings->reset("QuitAfterGameStop");
    }

    // Custom Commands
    bool custcmd = !isOverrideMode() || m_ui->customCommands->checked();

    if (isOverrideMode()) {
        settings->set("OverrideCommands", custcmd);
    }

    if (custcmd) {
        settings->set("PreLoadCommand", m_ui->customCommands->preLoadCommand());
        settings->set("PreLaunchCommand", m_ui->customCommands->prelaunchCommand());
        settings->set("WrapperCommand", m_ui->customCommands->wrapperCommand());
        settings->set("PostExitCommand", m_ui->customCommands->postexitCommand());
    } else {
        settings->reset("PreLoadCommand");
        settings->reset("PreLaunchCommand");
        settings->reset("WrapperCommand");
        settings->reset("PostExitCommand");
    }

    // Environment Variables
    auto env = !isOverrideMode() || m_ui->environmentVariables->override();

    if (isOverrideMode()) {
        settings->set("OverrideEnv", env);
    }

    if (env) {
        settings->set("Env", Json::fromMap(m_ui->environmentVariables->value()));
    } else {
        settings->reset("Env");
    }

    // Workarounds
    bool workarounds = !isOverrideMode() || m_ui->nativeWorkaroundsGroupBox->isChecked();

    if (isOverrideMode()) {
        settings->set("OverrideNativeWorkarounds", workarounds);
    }

    if (workarounds) {
        settings->set("UseNativeGLFW", m_ui->useNativeGLFWCheck->isChecked());
        settings->set("CustomGLFWPath", m_ui->lineEditGLFWPath->text());
        settings->set("UseNativeOpenAL", m_ui->useNativeOpenALCheck->isChecked());
        settings->set("CustomOpenALPath", m_ui->lineEditOpenALPath->text());
        settings->set("UseNativeSDL", m_ui->useNativeSDLCheck->isChecked());
        settings->set("CustomSDLPath", m_ui->lineEditSDLPath->text());
    } else {
        settings->reset("UseNativeGLFW");
        settings->reset("CustomGLFWPath");
        settings->reset("UseNativeOpenAL");
        settings->reset("CustomOpenALPath");
        settings->reset("UseNativeSDL");
        settings->reset("CustomSDLPath");
    }

    // Performance
    bool performance = !isOverrideMode() || m_ui->perfomanceGroupBox->isChecked();

    if (isOverrideMode()) {
        settings->set("OverridePerformance", performance);
    }

    if (performance) {
        settings->set("EnableFeralGamemode", m_ui->enableFeralGamemodeCheck->isChecked());
        settings->set("EnableMangoHud", m_ui->enableMangoHud->isChecked());
        settings->set("UseDiscreteGpu", m_ui->useDiscreteGpuCheck->isChecked());
        settings->set("UseZink", m_ui->useZink->isChecked());
    } else {
        settings->reset("EnableFeralGamemode");
        settings->reset("EnableMangoHud");
        settings->reset("UseDiscreteGpu");
        settings->reset("UseZink");
    }

    // Shared game options
    bool gameOptions = !isOverrideMode() || m_ui->gameOptionsGroupBox->isChecked();

    if (isOverrideMode()) {
        settings->set("OverrideGameOptionsProfile", gameOptions);
    }

    if (gameOptions) {
        // an empty id means no profile, which is a valid override too
        settings->set("GameOptionsProfile", m_ui->gameOptionsProfileComboBox->currentData().toString());
        settings->set("GameOptionsReviewChanges", m_ui->gameOptionsReviewCheck->isChecked());
    } else {
        settings->reset("GameOptionsProfile");
        settings->reset("GameOptionsReviewChanges");
    }

    // Game time
    bool gameTime = !isOverrideMode() || m_ui->gameTimeGroupBox->isChecked();

    if (isOverrideMode()) {
        settings->set("OverrideGameTime", gameTime);
    }

    if (m_instance != nullptr) {
        if (gameTime) {
            settings->set("CountGameTime", m_ui->countGameTime->isChecked());
        } else {
            settings->reset("CountGameTime");
        }
    }

    if (gameTime) {
        settings->set("ShowGameTime", m_ui->showGameTime->isChecked());
        settings->set("RecordGameTime", m_ui->recordGameTime->isChecked());
    } else {
        settings->reset("ShowGameTime");
        settings->reset("RecordGameTime");
    }

    if (!isOverrideMode()) {
        settings->set("ShowGlobalGameTime", m_ui->showGlobalGameTime->isChecked());
        settings->set("ShowGameTimeWithoutDays", m_ui->showGameTimeWithoutDays->isChecked());
    }

    if (m_instance != nullptr) {
        // Join server on launch
        bool joinServerOnLaunch = m_ui->serverJoinGroupBox->isChecked();
        settings->set("JoinServerOnLaunch", joinServerOnLaunch);
        if (joinServerOnLaunch) {
            if (m_ui->serverJoinAddressButton->isChecked() || !m_quickPlaySingleplayer) {
                settings->set("JoinServerOnLaunchAddress", m_ui->serverJoinAddress->text());
                settings->reset("JoinWorldOnLaunch");
            } else {
                settings->set("JoinWorldOnLaunch", m_ui->worldsCb->currentText());
                settings->reset("JoinServerOnLaunchAddress");
            }
        } else {
            settings->reset("JoinServerOnLaunchAddress");
            settings->reset("JoinWorldOnLaunch");
        }

        // Use an account for this instance
        bool useAccountForInstance = m_ui->instanceAccountGroupBox->isChecked();
        settings->set("UseAccountForInstance", useAccountForInstance);
        if (useAccountForInstance) {
            int accountIndex = m_ui->instanceAccountSelector->currentIndex();

            if (accountIndex != -1) {
                const MinecraftAccountPtr account = APPLICATION->accounts()->at(accountIndex);
                if (account != nullptr) {
                    settings->set("InstanceAccountId", account->profileId());
                }
            }
        } else {
            settings->reset("InstanceAccountId");
        }

        settings->set("UseLatestMinecraftVersion", m_ui->latestMCVersionGroupBox->isChecked());
        settings->set("UseLatestMinecraftVersionType", m_ui->releaseRadioButton->isChecked() ? "release" : "any");
    }

    bool overrideLegacySettings = !isOverrideMode() || m_ui->legacySettingsGroupBox->isChecked();

    if (isOverrideMode()) {
        settings->set("OverrideLegacySettings", overrideLegacySettings);
    }

    if (overrideLegacySettings) {
        settings->set("OnlineFixes", m_ui->onlineFixes->isChecked());
    } else {
        settings->reset("OnlineFixes");
    }

    if (m_javaSettings != nullptr) {
        m_javaSettings->saveSettings();
    }
}

void MinecraftSettingsWidget::openGlobalSettings()
{
    const QString id = m_ui->settingsTabs->currentWidget()->objectName();

    qDebug() << id;

    if (id == "javaPage") {
        APPLICATION->ShowGlobalSettings(this, "java-settings");
    } else {  // TODO select tab
        APPLICATION->ShowGlobalSettings(this, "minecraft-settings");
    }
}

void MinecraftSettingsWidget::populateGameOptionsProfiles(const QString& selectedId)
{
    auto* combo = m_ui->gameOptionsProfileComboBox;
    combo->blockSignals(true);
    combo->clear();
    combo->addItem(tr("None"), QString());
    for (const auto& profile : APPLICATION->gameOptionsProfiles()->profiles()) {
        auto label = profile.targetVersion.isEmpty() ? profile.name : tr("%1 (Minecraft %2)").arg(profile.name, profile.targetVersion);
        combo->addItem(label, profile.id);
    }
    auto index = combo->findData(selectedId);
    if (index < 0) {
        // keep a deleted profile selected instead of silently switching to another one
        combo->addItem(tr("Missing profile"), selectedId);
        index = combo->count() - 1;
    }
    combo->setCurrentIndex(index);
    combo->blockSignals(false);
    updateGameOptionsInfo();
}

std::pair<QString, QString> MinecraftSettingsWidget::inheritedGameOptionsProfile() const
{
    if (m_instance != nullptr) {
        auto* group = m_instance->groupSettings();
        if (group != nullptr && group->get("OverrideGameOptionsProfile").toBool()) {
            auto groupName = APPLICATION->instances()->getInstanceGroup(m_instance->id());
            return { group->get("GameOptionsProfile").toString(), tr("the group %1").arg(groupName) };
        }
    }
    return { APPLICATION->settings()->get("GameOptionsProfile").toString(), tr("the global settings") };
}

void MinecraftSettingsWidget::updateGameOptionsInfo()
{
    QStringList info;
    const auto profileId = m_ui->gameOptionsProfileComboBox->currentData().toString();
    const auto* profile = APPLICATION->gameOptionsProfiles()->profile(profileId);

    if (isOverrideMode() && !m_ui->gameOptionsGroupBox->isChecked()) {
        info << tr("Inherited from %1.").arg(inheritedGameOptionsProfile().second);
    }
    if (!profileId.isEmpty() && profile == nullptr) {
        info << tr("The selected profile no longer exists, so no game options are shared.");
    }
    if (m_instance != nullptr && profile != nullptr && !profile->targetVersion.isEmpty()) {
        auto instanceVersion = m_instance->getPackProfile()->getComponentVersion("net.minecraft");
        if (!instanceVersion.isEmpty() && instanceVersion != profile->targetVersion) {
            info << tr("This profile is meant for Minecraft %1, but this instance uses %2. "
                       "Options that changed between these versions may not carry over.")
                        .arg(profile->targetVersion, instanceVersion);
        }
    }

    m_ui->gameOptionsInfoLabel->setText(info.join(' '));
    m_ui->gameOptionsInfoLabel->setVisible(!info.isEmpty());
}

void MinecraftSettingsWidget::updateAccountsMenu(SettingsObject& settings) const
{
    m_ui->instanceAccountSelector->clear();
    auto* accounts = APPLICATION->accounts();
    int accountIndex = accounts->findAccountByProfileId(settings.get("InstanceAccountId").toString());

    for (int i = 0; i < accounts->count(); i++) {
        MinecraftAccountPtr account = accounts->at(i);

        QIcon face = account->getFace();

        if (face.isNull()) {
            face = QIcon::fromTheme("noaccount");
        }

        m_ui->instanceAccountSelector->addItem(face, account->profileName(), i);
        if (i == accountIndex) {
            m_ui->instanceAccountSelector->setCurrentIndex(i);
        }
    }
}

bool MinecraftSettingsWidget::isQuickPlaySupported()
{
    return m_instance->traits().contains("feature:is_quick_play_singleplayer");
}

void MinecraftSettingsWidget::saveSelectedLoaders()
{
    QStringList loaders;

    if (m_ui->neoForge->isChecked()) {
        loaders << getModLoaderAsString(ModPlatform::NeoForge);
    }
    if (m_ui->forge->isChecked()) {
        loaders << getModLoaderAsString(ModPlatform::Forge);
    }
    if (m_ui->fabric->isChecked()) {
        loaders << getModLoaderAsString(ModPlatform::Fabric);
    }
    if (m_ui->quilt->isChecked()) {
        loaders << getModLoaderAsString(ModPlatform::Quilt);
    }
    if (m_ui->liteLoader->isChecked()) {
        loaders << getModLoaderAsString(ModPlatform::LiteLoader);
    }
    if (m_ui->babric->isChecked()) {
        loaders << getModLoaderAsString(ModPlatform::Babric);
    }
    if (m_ui->btaBabric->isChecked()) {
        loaders << getModLoaderAsString(ModPlatform::BTA);
    }
    if (m_ui->legacyFabric->isChecked()) {
        loaders << getModLoaderAsString(ModPlatform::LegacyFabric);
    }
    if (m_ui->ornithe->isChecked()) {
        loaders << getModLoaderAsString(ModPlatform::Ornithe);
    }
    if (m_ui->rift->isChecked()) {
        loaders << getModLoaderAsString(ModPlatform::Rift);
    }

    m_instance->settings()->set("ModDownloadLoaders", Json::fromStringList(loaders));
}

void MinecraftSettingsWidget::saveDataPacksPath()
{
    if (QDir::separator() != '/') {
        m_ui->dataPacksPathEdit->setText(m_ui->dataPacksPathEdit->text().replace(QDir::separator(), '/'));
    }

    m_instance->settings()->set("GlobalDataPacksPath", m_ui->dataPacksPathEdit->text());
}

void MinecraftSettingsWidget::selectDataPacksFolder()
{
    QString path = QFileDialog::getExistingDirectory(this, tr("Select Global Data Packs Folder"), m_instance->gameRoot());

    if (path.isEmpty()) {
        return;
    }

    // if it's inside the instance dir, set path relative to .minecraft
    // (so that if it's directly in instance dir it will still lead with .. but more than two levels up are kept absolute)

    const QUrl instanceRootUrl = QUrl::fromLocalFile(m_instance->instanceRoot());
    const QUrl pathUrl = QUrl::fromLocalFile(path);

    if (instanceRootUrl.isParentOf(pathUrl)) {
        path = QDir(m_instance->gameRoot()).relativeFilePath(path);
    }

    m_ui->dataPacksPathEdit->setText(path);
    m_instance->settings()->set("GlobalDataPacksPath", path);
}
