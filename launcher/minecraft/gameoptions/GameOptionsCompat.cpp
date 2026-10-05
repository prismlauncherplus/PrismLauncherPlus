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

#include <QRegularExpression>
#include <QSet>

#include "OptionsFile.h"

namespace GameOptionsCompat {

namespace {
const QString keybindPrefix = QStringLiteral("key_");
const QString numberBand = QStringLiteral("number");
const QString quotedBand = QStringLiteral("quoted");
const QString structuredBand = QStringLiteral("structured");
const QString wordBand = QStringLiteral("word");
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
            format.keybinds = bandOf(key, *existing.value(key)) == numberBand ? KeybindFormat::Codes : KeybindFormat::Names;
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
        // per instance state rather than settings.
        // dismissed prompts and tutorials (onboardAccessibility, tutorialStep, ...) are shared on purpose, so they are only shown once
        "lastServer",
        // Minecraft's crash detection for the game folder
        "startedCleanly",
    };
    return !key.isEmpty() && !instanceSpecific.contains(key);
}

bool isKeybind(const QString& key)
{
    return key.startsWith(keybindPrefix);
}

bool isNumber(const QString& value)
{
    static const QRegularExpression number(QStringLiteral("^[+-]?(\\d+\\.?\\d*|\\.\\d+)([eE][+-]?\\d+)?$"));
    return number.match(value.trimmed()).hasMatch();
}

QString bandOf(const QString& key, const QString& value)
{
    auto trimmed = value.trimmed();
    if (isKeybind(key)) {
        // Forge adds modifiers to keybinds ("17:SHIFT")
        trimmed = trimmed.section(':', 0, 0);
    }
    if (isNumber(trimmed)) {
        return numberBand;
    }
    if (trimmed.size() >= 2 && trimmed.startsWith('"') && trimmed.endsWith('"')) {
        return quotedBand;
    }
    if (trimmed.startsWith('[') || trimmed.startsWith('{')) {
        return structuredBand;
    }
    return wordBand;
}

std::optional<QString> keybindBand(const ClientFormat& client)
{
    switch (client.keybinds) {
        case KeybindFormat::Codes:
            return numberBand;
        case KeybindFormat::Names:
            return wordBand;
        case KeybindFormat::Unknown:
            break;
    }
    return std::nullopt;
}

}  // namespace GameOptionsCompat
