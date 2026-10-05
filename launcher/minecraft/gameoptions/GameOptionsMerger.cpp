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

#include <algorithm>
#include <cmath>

namespace GameOptionsMerger {

namespace {
// the band of the profile's values the client gets
std::optional<QString> chooseBand(const QString& key,
                                  const QMap<QString, GameOptionValue>& variants,
                                  const GameOptionsCompat::ClientFormat& client,
                                  const std::optional<QString>& existing)
{
    // the client wrote the option itself, so it uses that shape
    if (existing) {
        return GameOptionsCompat::bandOf(key, *existing);
    }
    if (GameOptionsCompat::isKeybind(key)) {
        return GameOptionsCompat::keybindBand(client);
    }
    if (variants.size() == 1) {
        return variants.firstKey();
    }
    // the value written by the closest Minecraft version, or the most recent one if the version isn't known
    std::optional<QString> best;
    for (auto iter = variants.begin(); iter != variants.end(); ++iter) {
        if (!best) {
            best = iter.key();
            continue;
        }
        const auto& current = variants[*best];
        if (client.dataVersion && iter->dataVersion && current.dataVersion) {
            if (std::abs(*iter->dataVersion - *client.dataVersion) < std::abs(*current.dataVersion - *client.dataVersion)) {
                best = iter.key();
            }
        } else if (client.dataVersion && iter->dataVersion && !current.dataVersion) {
            best = iter.key();
        } else if (!client.dataVersion && iter->updated > current.updated) {
            best = iter.key();
        }
    }
    return best;
}

bool isSameValue(const QString& a, const QString& b)
{
    if (a == b) {
        return true;
    }
    bool aIsNumber = false;
    bool bIsNumber = false;
    const double aNumber = a.trimmed().toDouble(&aIsNumber);
    const double bNumber = b.trimmed().toDouble(&bIsNumber);
    if (!aIsNumber || !bIsNumber) {
        return false;
    }
    // e.g. "0.7262599031690141" and "0.72626", the same setting written by versions storing it with different precision
    return std::abs(aNumber - bNumber) <= 1e-5 * std::max({ 1.0, std::abs(aNumber), std::abs(bNumber) });
}
}  // namespace

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
        auto band = chooseBand(key, *option, client, current.value(key));
        auto value = band ? option->find(*band) : option->end();
        if (value == option->end()) {
            result.skipped.append(key);
            continue;
        }
        result.file.set(key, value->value);
        result.applied.append(key);
        result.snapshot.insert(key, value->value);
    }

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
        } else if (!isSameValue(*before, iter.value())) {
            changes.append({ iter.key(), *before, iter.value() });
        }
    }
    return changes;
}

}  // namespace GameOptionsMerger
