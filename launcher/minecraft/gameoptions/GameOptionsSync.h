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

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <functional>
#include <optional>

#include "GameOptionsCompat.h"
#include "MessageLevel.h"
#include "Result.h"

class InstanceList;
class MinecraftInstance;

/*!
 * Keeping an instance's options.txt in sync with its game options profile: the profile is applied before the game starts,
 * and the options changed in game are written back to the profile when it stops.
 */
namespace GameOptionsSync {

/// What was applied to an instance, to find the changes once the game stops. It is also saved next to the instance,
/// so the changes can still be collected if the launcher crashes or quits while the game is running.
struct Session {
    QString profileId;
    GameOptionsCompat::ClientFormat client;
    QMap<QString, QString> snapshot;

    QJsonObject toJson() const;
    static Result<Session> fromJson(const QJsonObject& json);
};

using Logger = std::function<void(const QString& message, MessageLevel level)>;

QString optionsPath(MinecraftInstance* instance);

/// The data version of the instance's Minecraft version, from the client jar (Minecraft 1.14 and later have it).
/// Needed when there is no options.txt yet to tell which version wrote it.
std::optional<int> dataVersionFromJar(MinecraftInstance* instance);

/// Apply the instance's profile to its options.txt; returns the session if anything is to be synced when the game stops
std::optional<Session> start(MinecraftInstance* instance, const Logger& log);

/// Write the options changed since the session started back to the profile, or let the user review them first
void finish(MinecraftInstance* instance, const Session& session, const Logger& log);

/// finish the sessions left over from a launcher that crashed or quit while the game was running
void recoverSessions(InstanceList* instances);

}  // namespace GameOptionsSync
