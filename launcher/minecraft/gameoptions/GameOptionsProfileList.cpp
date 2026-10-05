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
#include "GameOptionsProfileList.h"

#include <QDebug>
#include <QDir>
#include <QJsonDocument>
#include <QLockFile>
#include <QUuid>

#include "FileSystem.h"

namespace {
// how long to wait for another instance writing to the same profile
constexpr int lockTimeoutMs = 5000;
}  // namespace

GameOptionsProfileList::GameOptionsProfileList(QString directory, QObject* parent)
    : QAbstractListModel(parent), m_directory(std::move(directory))
{}

void GameOptionsProfileList::load()
{
    beginResetModel();
    m_profiles.clear();
    const auto files = QDir(m_directory).entryInfoList({ "*.json" }, QDir::Files);
    for (const auto& file : files) {
        auto profile = readProfile(file.absoluteFilePath());
        if (!profile) {
            qWarning() << "Ignoring game options profile" << file.absoluteFilePath() << ":" << profile.error();
            continue;
        }
        if (profilePath(profile->id) != file.absoluteFilePath()) {
            qWarning() << "Ignoring game options profile" << file.absoluteFilePath() << ": its id" << profile->id
                       << "doesn't match the file name";
            continue;
        }
        m_profiles.append(*profile);
    }
    sortProfiles();
    endResetModel();
}

int GameOptionsProfileList::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_profiles.size());
}

QVariant GameOptionsProfileList::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_profiles.size()) {
        return {};
    }
    const auto& profile = m_profiles[index.row()];
    switch (role) {
        case Qt::DisplayRole:
            return profile.name;
        case IdRole:
            return profile.id;
        case TargetVersionRole:
            return profile.targetVersion;
        default:
            return {};
    }
}

const GameOptionsProfile* GameOptionsProfileList::profile(const QString& id) const
{
    auto index = indexOf(id);
    return index < 0 ? nullptr : &m_profiles[index];
}

Result<QString> GameOptionsProfileList::createProfile(const QString& name, const QString& targetVersion)
{
    GameOptionsProfile profile;
    profile.name = name;
    profile.targetVersion = targetVersion;
    return addProfile(std::move(profile));
}

Result<QString> GameOptionsProfileList::duplicateProfile(const QString& id, const QString& name)
{
    const auto* original = profile(id);
    if (!original) {
        return std::unexpected(QString("there is no game options profile %1").arg(id));
    }
    // copy what is on disk, which may have changes this list doesn't know about yet
    auto current = readProfile(profilePath(id));
    GameOptionsProfile copy = current ? *current : *original;
    copy.name = name;
    return addProfile(std::move(copy));
}

Result<> GameOptionsProfileList::removeProfile(const QString& id)
{
    auto index = indexOf(id);
    if (index < 0) {
        return std::unexpected(QString("there is no game options profile %1").arg(id));
    }
    auto path = profilePath(id);
    if (QFile::exists(path) && !QFile::remove(path)) {
        return std::unexpected(QString("could not delete %1").arg(path));
    }
    beginRemoveRows(QModelIndex(), static_cast<int>(index), static_cast<int>(index));
    m_profiles.removeAt(index);
    endRemoveRows();
    return {};
}

Result<> GameOptionsProfileList::modifyProfile(const QString& id, const Modification& modification)
{
    auto index = indexOf(id);
    if (index < 0) {
        return std::unexpected(QString("there is no game options profile %1").arg(id));
    }

    auto path = profilePath(id);
    QLockFile lock(path + ".lock");
    if (!lock.tryLock(lockTimeoutMs)) {
        return std::unexpected(QString("could not lock %1, it is being changed by something else").arg(path));
    }

    // start from what is on disk, another instance may have written changes since it was loaded
    GameOptionsProfile profile = m_profiles[index];
    if (QFile::exists(path)) {
        auto current = readProfile(path);
        if (!current) {
            return std::unexpected(current.error());
        }
        profile = *current;
    }

    modification(profile);
    profile.id = id;

    if (auto result = writeProfile(profile); !result) {
        return result;
    }
    m_profiles[index] = profile;
    // a rename can change the order
    emit layoutAboutToBeChanged();
    sortProfiles();
    emit layoutChanged();
    emit dataChanged(this->index(0), this->index(static_cast<int>(m_profiles.size() - 1)));
    return {};
}

Result<> GameOptionsProfileList::setProfileInfo(const QString& id, const QString& name, const QString& targetVersion)
{
    return modifyProfile(id, [&name, &targetVersion](GameOptionsProfile& profile) {
        profile.name = name;
        profile.targetVersion = targetVersion;
    });
}

Result<> GameOptionsProfileList::applyChanges(const QString& id,
                                              const QList<GameOptionChange>& changes,
                                              const GameOptionsCompat::ClientFormat& client)
{
    if (changes.isEmpty()) {
        return {};
    }
    const auto now = QDateTime::currentDateTimeUtc();
    return modifyProfile(id, [&changes, &client, &now](GameOptionsProfile& profile) { profile.applyChanges(changes, client, now); });
}

QString GameOptionsProfileList::profilePath(const QString& id) const
{
    return QDir(m_directory).absoluteFilePath(id + ".json");
}

Result<GameOptionsProfile> GameOptionsProfileList::readProfile(const QString& path) const
{
    auto data = FS::read(path);
    if (!data) {
        return std::unexpected(data.error());
    }
    QJsonParseError error;
    auto document = QJsonDocument::fromJson(*data, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return std::unexpected(QString("invalid JSON: %1").arg(error.errorString()));
    }
    return GameOptionsProfile::fromJson(document.object());
}

Result<> GameOptionsProfileList::writeProfile(const GameOptionsProfile& profile) const
{
    return FS::write(profilePath(profile.id), QJsonDocument(profile.toJson()).toJson());
}

Result<QString> GameOptionsProfileList::addProfile(GameOptionsProfile profile)
{
    profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (auto result = writeProfile(profile); !result) {
        return std::unexpected(result.error());
    }
    beginResetModel();
    m_profiles.append(profile);
    sortProfiles();
    endResetModel();
    return profile.id;
}

qsizetype GameOptionsProfileList::indexOf(const QString& id) const
{
    for (qsizetype i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles[i].id == id) {
            return i;
        }
    }
    return -1;
}

void GameOptionsProfileList::sortProfiles()
{
    std::ranges::stable_sort(m_profiles, [](const GameOptionsProfile& a, const GameOptionsProfile& b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });
}
