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
#include "GameOptionsProfile.h"

#include <QJsonArray>

#include "OptionsFile.h"

namespace {
constexpr int formatVersion = 1;
}

std::optional<GameOptionValue> GameOptionsProfile::value(const QString& key, const QString& band) const
{
    auto option = options.find(key);
    if (option == options.end()) {
        return std::nullopt;
    }
    auto bandValue = option->find(band);
    if (bandValue == option->end()) {
        return std::nullopt;
    }
    return *bandValue;
}

void GameOptionsProfile::setValue(const QString& key, const QString& value, std::optional<int> dataVersion, const QDateTime& updated)
{
    options[key][GameOptionsCompat::bandOf(key, value)] = { value, dataVersion, updated };
}

void GameOptionsProfile::applyChanges(const QList<GameOptionChange>& changes,
                                      const GameOptionsCompat::ClientFormat& client,
                                      const QDateTime& updated)
{
    for (const auto& change : changes) {
        if (GameOptionsCompat::isShareable(change.key)) {
            setValue(change.key, change.newValue, client.dataVersion, updated);
        }
    }
}

void GameOptionsProfile::importFrom(const OptionsFile& file, const GameOptionsCompat::ClientFormat& client, const QDateTime& updated)
{
    const auto values = file.values();
    for (auto iter = values.begin(); iter != values.end(); ++iter) {
        if (GameOptionsCompat::isShareable(iter.key())) {
            setValue(iter.key(), iter.value(), client.dataVersion, updated);
        }
    }
}

QJsonObject GameOptionsProfile::toJson() const
{
    QJsonObject optionsObject;
    for (auto option = options.begin(); option != options.end(); ++option) {
        QJsonArray values;
        for (const auto& value : *option) {
            QJsonObject valueObject{ { "value", value.value }, { "updated", value.updated.toString(Qt::ISODateWithMs) } };
            if (value.dataVersion) {
                valueObject.insert("dataVersion", *value.dataVersion);
            }
            values.append(valueObject);
        }
        optionsObject.insert(option.key(), values);
    }

    QJsonObject json{ { "formatVersion", formatVersion }, { "id", id }, { "name", name }, { "options", optionsObject } };
    if (!targetVersion.isEmpty()) {
        json.insert("targetVersion", targetVersion);
    }
    return json;
}

Result<GameOptionsProfile> GameOptionsProfile::fromJson(const QJsonObject& json)
{
    if (json.value("formatVersion").toInt() != formatVersion) {
        return std::unexpected(QString("unsupported format version %1").arg(json.value("formatVersion").toInt()));
    }

    GameOptionsProfile profile;
    profile.id = json.value("id").toString();
    profile.name = json.value("name").toString();
    profile.targetVersion = json.value("targetVersion").toString();
    if (profile.id.isEmpty()) {
        return std::unexpected(QString("the profile has no id"));
    }

    const auto optionsObject = json.value("options").toObject();
    for (auto option = optionsObject.begin(); option != optionsObject.end(); ++option) {
        for (const auto& valueJson : option.value().toArray()) {
            const auto valueObject = valueJson.toObject();
            if (!valueObject.value("value").isString()) {
                continue;
            }
            std::optional<int> dataVersion;
            if (valueObject.contains("dataVersion")) {
                dataVersion = valueObject.value("dataVersion").toInt();
            }
            auto updated = QDateTime::fromString(valueObject.value("updated").toString(), Qt::ISODateWithMs);
            // the band is derived from the value, so a hand edited file can't put a value in the wrong band
            const auto value = valueObject.value("value").toString();
            // two stored values in the same band (e.g. after a change of the band rules): keep the newer one
            if (auto existing = profile.value(option.key(), GameOptionsCompat::bandOf(option.key(), value));
                existing && existing->updated > updated) {
                continue;
            }
            profile.setValue(option.key(), value, dataVersion, updated);
        }
    }
    return profile;
}
