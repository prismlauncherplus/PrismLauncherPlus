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

#include <QAbstractListModel>
#include <QList>
#include <functional>

#include "GameOptionsProfile.h"
#include "Result.h"

/*!
 * The game options profiles, stored as one JSON file per profile in a directory.
 *
 * All changes are read-modify-write cycles under a file lock: the profile is read from disk again before it is changed,
 * so instances writing back changes at the same time only overwrite the options they changed themselves.
 */
class GameOptionsProfileList : public QAbstractListModel {
    Q_OBJECT
   public:
    enum Roles { IdRole = Qt::UserRole, TargetVersionRole };

    explicit GameOptionsProfileList(QString directory, QObject* parent = nullptr);

    /// (re)load all profiles from the directory
    void load();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    const QList<GameOptionsProfile>& profiles() const { return m_profiles; }
    /// nullptr if there is no profile with the id, e.g. because it was deleted
    const GameOptionsProfile* profile(const QString& id) const;

    /// @return the id of the new profile
    Result<QString> createProfile(const QString& name, const QString& targetVersion = QString());
    /// copy all the options of a profile into a new one; @return the id of the new profile
    Result<QString> duplicateProfile(const QString& id, const QString& name);
    Result<> removeProfile(const QString& id);

    using Modification = std::function<void(GameOptionsProfile&)>;
    /// change a profile safely, see the class description
    Result<> modifyProfile(const QString& id, const Modification& modification);

    Result<> setProfileInfo(const QString& id, const QString& name, const QString& targetVersion);
    Result<> applyChanges(const QString& id, const QList<GameOptionChange>& changes, const GameOptionsCompat::ClientFormat& client);

   signals:
    /// the profile was deleted (in the launcher or on disk); whatever uses it should use no profile
    void profileRemoved(const QString& id);

   private:
    QString profilePath(const QString& id) const;
    Result<GameOptionsProfile> readProfile(const QString& path) const;
    Result<> writeProfile(const GameOptionsProfile& profile) const;
    Result<QString> addProfile(GameOptionsProfile profile);
    qsizetype indexOf(const QString& id) const;
    void sortProfiles();
    void resortKeepingIndexes();

    QString m_directory;
    QList<GameOptionsProfile> m_profiles;
};
