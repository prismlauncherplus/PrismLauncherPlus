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
#include "GameOptionsMerger.h"

namespace GameOptionsMerger {

ApplyResult apply(const GameOptionsProfile& profile, const OptionsFile& current, const GameOptionsCompat::ClientFormat& client)
{
    ApplyResult result{ current, {}, {}, {} };

    // without a version, Minecraft treats the file as written by a very old version and "upgrades" the values,
    // so a new file has to say which version it is for
    if (result.file.isEmpty() && client.dataVersion) {
        result.file.set("version", QString::number(*client.dataVersion));
    }

    for (auto option = profile.options.begin(); option != profile.options.end(); ++option) {
        const auto& key = option.key();
        if (!GameOptionsCompat::isShareable(key)) {
            continue;
        }
        auto band = GameOptionsCompat::bandFor(key, client);
        auto value = band ? profile.value(key, *band) : std::nullopt;
        if (!value) {
            result.skipped.append(key);
            continue;
        }
        result.file.set(key, value->value);
        result.applied.append(key);
    }

    result.snapshot = snapshotOf(result.file);
    return result;
}

QMap<QString, QString> snapshotOf(const OptionsFile& file)
{
    QMap<QString, QString> snapshot;
    const auto values = file.values();
    for (auto iter = values.begin(); iter != values.end(); ++iter) {
        if (GameOptionsCompat::isShareable(iter.key())) {
            snapshot.insert(iter.key(), iter.value());
        }
    }
    return snapshot;
}

QList<GameOptionChange> collectChanges(const QMap<QString, QString>& snapshot, const OptionsFile& after)
{
    QList<GameOptionChange> changes;
    const auto values = snapshotOf(after);
    for (auto iter = values.begin(); iter != values.end(); ++iter) {
        auto before = snapshot.find(iter.key());
        if (before == snapshot.end()) {
            changes.append({ iter.key(), std::nullopt, iter.value() });
        } else if (*before != iter.value()) {
            changes.append({ iter.key(), *before, iter.value() });
        }
    }
    return changes;
}

}  // namespace GameOptionsMerger
