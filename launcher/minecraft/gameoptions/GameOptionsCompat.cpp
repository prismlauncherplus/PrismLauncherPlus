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
#include "GameOptionsCompat.h"

#include <QSet>

#include "OptionsFile.h"

namespace GameOptionsCompat {

namespace {
const QString keybindPrefix = QStringLiteral("key_");
const QString sharedBand;
const QString keyCodesBand = QStringLiteral("keybind-codes");
const QString keyNamesBand = QStringLiteral("keybind-names");

bool isKeyCode(const QString& value)
{
    bool ok = false;
    value.trimmed().toInt(&ok);
    return ok;
}
}  // namespace

ClientFormat ClientFormat::detect(const OptionsFile& existing, std::optional<int> fallbackDataVersion)
{
    ClientFormat format;
    format.dataVersion = existing.dataVersion();
    if (!format.dataVersion) {
        format.dataVersion = fallbackDataVersion;
    }

    // the keybinds the client wrote itself tell their format for sure
    for (const auto& key : existing.keys()) {
        if (isKeybind(key)) {
            format.keybinds = isKeyCode(*existing.value(key)) ? KeybindFormat::Codes : KeybindFormat::Names;
            return format;
        }
    }
    if (format.dataVersion) {
        format.keybinds = *format.dataVersion >= KeybindNamesDataVersion ? KeybindFormat::Names : KeybindFormat::Codes;
    }
    return format;
}

bool isShareable(const QString& key)
{
    static const QSet<QString> instanceSpecific = {
        // written by every client for itself
        "version",
        // depend on the resource packs installed in the instance
        "resourcePacks",
        "incompatibleResourcePacks",
        // per instance state rather than settings
        "lastServer",
        "tutorialStep",
        "joinedFirstServer",
        "onboardAccessibility",
        "skipMultiplayerWarning",
        "skipRealms32bitWarning",
        "hideBundleTutorial",
        // Minecraft's crash detection for the game folder
        "startedCleanly",
    };
    return !key.isEmpty() && !instanceSpecific.contains(key);
}

bool isKeybind(const QString& key)
{
    return key.startsWith(keybindPrefix);
}

QString bandOf(const QString& key, const QString& value)
{
    if (isKeybind(key)) {
        return isKeyCode(value) ? keyCodesBand : keyNamesBand;
    }
    return sharedBand;
}

std::optional<QString> bandFor(const QString& key, const ClientFormat& client)
{
    if (!isKeybind(key)) {
        return sharedBand;
    }
    switch (client.keybinds) {
        case KeybindFormat::Codes:
            return keyCodesBand;
        case KeybindFormat::Names:
            return keyNamesBand;
        case KeybindFormat::Unknown:
            break;
    }
    return std::nullopt;
}

}  // namespace GameOptionsCompat
