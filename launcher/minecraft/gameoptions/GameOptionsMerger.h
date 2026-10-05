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

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

#include "GameOptionsCompat.h"
#include "GameOptionsProfile.h"
#include "OptionsFile.h"

/*!
 * Merging a profile into a client's options file before the game starts, and finding what the player changed afterwards.
 */
namespace GameOptionsMerger {

struct ApplyResult {
    OptionsFile file;
    /// the shareable options of the merged file, to find out what changed in game
    QMap<QString, QString> snapshot;
    /// options taken from the profile
    QStringList applied;
    /// options of the profile that have no value the client can read (e.g. keybinds of an older Minecraft version)
    QStringList skipped;
};

ApplyResult apply(const GameOptionsProfile& profile, const OptionsFile& current, const GameOptionsCompat::ClientFormat& client);

/// the shareable options of the file
QMap<QString, QString> snapshotOf(const OptionsFile& file);

/// the shareable options that were added or changed since the snapshot was taken; removed options are ignored
QList<GameOptionChange> collectChanges(const QMap<QString, QString>& snapshot, const OptionsFile& after);

}  // namespace GameOptionsMerger
