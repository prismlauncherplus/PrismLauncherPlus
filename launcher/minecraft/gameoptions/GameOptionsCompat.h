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
#pragma once

#include <QString>
#include <optional>

class OptionsFile;

/*!
 * Rules for sharing options.txt entries between Minecraft versions.
 *
 * Some options changed the format of their values between versions. Their values are kept in separate "format bands",
 * and a client only gets values from its own band, so e.g. a 1.12 and a 1.20 instance don't overwrite each other's keybinds.
 * Options without known format changes all share a single band.
 */
namespace GameOptionsCompat {

/// The data version of the snapshot that moved to LWJGL 3 (17w43a, the first 1.13 snapshot),
/// which changed keybinds from LWJGL 2 key codes ("key_key.forward:17") to key names ("key_key.forward:key.keyboard.w")
constexpr int KeybindNamesDataVersion = 1444;

enum class KeybindFormat { Unknown, Codes, Names };

/// what a specific Minecraft client reads and writes
struct ClientFormat {
    std::optional<int> dataVersion;
    KeybindFormat keybinds = KeybindFormat::Unknown;

    /// Detect the format from the client's current options file, the most reliable source as the client wrote it itself.
    /// `fallbackDataVersion` is used when the file doesn't say (e.g. there is no file yet).
    static ClientFormat detect(const OptionsFile& existing, std::optional<int> fallbackDataVersion = std::nullopt);
};

/// whether the option is shared at all; things like resource packs or the last server are tied to a single instance
bool isShareable(const QString& key);

/// whether the option's values depend on the Minecraft version
bool isKeybind(const QString& key);

/// the format band of a value, as written by some client
QString bandOf(const QString& key, const QString& value);

/// the format band the client reads, or nothing if it can't be determined (values of the option must not be applied then)
std::optional<QString> bandFor(const QString& key, const ClientFormat& client);

}  // namespace GameOptionsCompat
