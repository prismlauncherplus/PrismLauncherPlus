// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Calum Hansen
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
 */

#include "InheritableSettings.h"

#include "SettingsObject.h"

namespace InheritableSettings {

void registerCommon(SettingsObject* settings, const ParentLookup& parent)
{
    // Game time
    auto gameTimeOverride = settings->registerSetting("OverrideGameTime", false);
    settings->registerOverride(parent("ShowGameTime"), gameTimeOverride);
    settings->registerOverride(parent("RecordGameTime"), gameTimeOverride);

    // Custom Commands
    auto commandSetting = settings->registerSetting({ "OverrideCommands", "OverrideLaunchCmd" }, false);
    settings->registerOverride(parent("PreLoadCommand"), commandSetting);
    settings->registerOverride(parent("PreLaunchCommand"), commandSetting);
    settings->registerOverride(parent("WrapperCommand"), commandSetting);
    settings->registerOverride(parent("PostExitCommand"), commandSetting);

    // Console
    auto consoleSetting = settings->registerSetting("OverrideConsole", false);
    settings->registerOverride(parent("ShowConsole"), consoleSetting);
    settings->registerOverride(parent("AutoCloseConsole"), consoleSetting);
    settings->registerOverride(parent("ShowConsoleOnError"), consoleSetting);
    settings->registerOverride(parent("LogPrePostOutput"), consoleSetting);
}

void registerMinecraft(SettingsObject* settings, const ParentLookup& parent)
{
    // Java Settings
    auto locationOverride = settings->registerSetting("OverrideJavaLocation", false);
    auto argsOverride = settings->registerSetting("OverrideJavaArgs", false);
    settings->registerOverride(parent("JavaPath"), locationOverride);
    settings->registerOverride(parent("JvmArgs"), argsOverride);
    settings->registerOverride(parent("IgnoreJavaCompatibility"), locationOverride);

    // special!
    settings->registerPassthrough(parent("JavaSignature"), locationOverride);
    settings->registerPassthrough(parent("JavaArchitecture"), locationOverride);
    settings->registerPassthrough(parent("JavaRealArchitecture"), locationOverride);
    settings->registerPassthrough(parent("JavaVersion"), locationOverride);
    settings->registerPassthrough(parent("JavaVendor"), locationOverride);

    // Window Size
    auto windowSetting = settings->registerSetting("OverrideWindow", false);
    settings->registerOverride(parent("LaunchMaximized"), windowSetting);
    settings->registerOverride(parent("MinecraftWinWidth"), windowSetting);
    settings->registerOverride(parent("MinecraftWinHeight"), windowSetting);

    // Memory
    auto memorySetting = settings->registerSetting("OverrideMemory", false);
    settings->registerOverride(parent("MinMemAlloc"), memorySetting);
    settings->registerOverride(parent("MaxMemAlloc"), memorySetting);
    settings->registerOverride(parent("PermGen"), memorySetting);
    settings->registerOverride(parent("LowMemWarning"), memorySetting);

    // Native library workarounds
    auto nativeLibraryWorkaroundsOverride = settings->registerSetting("OverrideNativeWorkarounds", false);
    settings->registerOverride(parent("UseNativeOpenAL"), nativeLibraryWorkaroundsOverride);
    settings->registerOverride(parent("CustomOpenALPath"), nativeLibraryWorkaroundsOverride);
    settings->registerOverride(parent("UseNativeGLFW"), nativeLibraryWorkaroundsOverride);
    settings->registerOverride(parent("CustomGLFWPath"), nativeLibraryWorkaroundsOverride);
    settings->registerOverride(parent("UseNativeSDL"), nativeLibraryWorkaroundsOverride);
    settings->registerOverride(parent("CustomSDLPath"), nativeLibraryWorkaroundsOverride);

    // Performance related options
    auto performanceOverride = settings->registerSetting("OverridePerformance", false);
    settings->registerOverride(parent("EnableFeralGamemode"), performanceOverride);
    settings->registerOverride(parent("EnableMangoHud"), performanceOverride);
    settings->registerOverride(parent("UseDiscreteGpu"), performanceOverride);
    settings->registerOverride(parent("UseZink"), performanceOverride);

    // Miscellaneous
    auto miscellaneousOverride = settings->registerSetting("OverrideMiscellaneous", false);
    settings->registerOverride(parent("CloseAfterLaunch"), miscellaneousOverride);
    settings->registerOverride(parent("QuitAfterGameStop"), miscellaneousOverride);

    // Legacy-related options
    auto legacySettings = settings->registerSetting("OverrideLegacySettings", false);
    settings->registerOverride(parent("OnlineFixes"), legacySettings);

    auto envSetting = settings->registerSetting("OverrideEnv", false);
    settings->registerOverride(parent("Env"), envSetting);

    // Shared game options (options.txt) profile, overriding with an empty id means no profile
    auto gameOptionsSetting = settings->registerSetting("OverrideGameOptionsProfile", false);
    settings->registerOverride(parent("GameOptionsProfile"), gameOptionsSetting);
    settings->registerOverride(parent("GameOptionsReviewChanges"), gameOptionsSetting);
}

QStringList gateIds()
{
    return { "OverrideGameTime",       "OverrideCommands", "OverrideConsole",           "OverrideJavaLocation", "OverrideJavaArgs",
             "OverrideWindow",         "OverrideMemory",   "OverrideNativeWorkarounds", "OverridePerformance",  "OverrideMiscellaneous",
             "OverrideLegacySettings", "OverrideEnv",      "OverrideGameOptionsProfile" };
}

}  // namespace InheritableSettings
