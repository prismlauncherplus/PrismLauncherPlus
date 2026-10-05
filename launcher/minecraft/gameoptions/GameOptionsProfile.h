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

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <optional>

#include "GameOptionsCompat.h"
#include "Result.h"

class OptionsFile;

struct GameOptionValue {
    QString value;
    /// data version of the client that wrote the value, if known
    std::optional<int> dataVersion;
    QDateTime updated;
};

/// an option that was changed in game, to be written back to the profile
struct GameOptionChange {
    QString key;
    /// the value before the game started, if the option existed
    std::optional<QString> oldValue;
    QString newValue;
};

/*!
 * A named set of Minecraft client options (options.txt entries) shared by the instances that use it.
 * Options whose format depends on the Minecraft version keep one value per format band (see GameOptionsCompat).
 */
struct GameOptionsProfile {
    QString id;
    QString name;
    /// the Minecraft version the profile is meant for, empty if it isn't meant for a specific one
    QString targetVersion;
    /// key -> format band -> value
    QMap<QString, QMap<QString, GameOptionValue>> options;

    std::optional<GameOptionValue> value(const QString& key, const QString& band) const;
    /// store a value written by a client, in the band the value belongs to
    void setValue(const QString& key, const QString& value, std::optional<int> dataVersion, const QDateTime& updated);
    void applyChanges(const QList<GameOptionChange>& changes, const GameOptionsCompat::ClientFormat& client, const QDateTime& updated);
    /// take over all the shareable options of a client's options file
    void importFrom(const OptionsFile& file, const GameOptionsCompat::ClientFormat& client, const QDateTime& updated);

    QJsonObject toJson() const;
    static Result<GameOptionsProfile> fromJson(const QJsonObject& json);
};
