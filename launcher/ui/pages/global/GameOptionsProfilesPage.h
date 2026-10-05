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

#include <QWidget>

#include "ui/pages/BasePage.h"

class QLabel;
class QListView;
class QPushButton;
class QTreeWidget;

/// Launcher settings page to manage the game options profiles shared between instances
class GameOptionsProfilesPage : public QWidget, public BasePage {
    Q_OBJECT

   public:
    explicit GameOptionsProfilesPage(QWidget* parent = nullptr);

    QString displayName() const override { return tr("Game Options"); }
    QIcon icon() const override { return QIcon::fromTheme("settings"); }
    QString id() const override { return "game-options"; }
    QString helpPage() const override { return "Game-options-profiles"; }

   private:
    /// what uses a profile: the global default, groups and instances
    struct Usage {
        bool global = false;
        QStringList groups;
        QStringList instances;  // ids
        bool isEmpty() const { return !global && groups.isEmpty() && instances.isEmpty(); }
        QString describe() const;
    };
    Usage usageOf(const QString& profileId) const;

    QString selectedId() const;
    void select(const QString& profileId);
    void updateDetails();
    void showError(const QString& title, const QString& error);

    void createProfile();
    void duplicateProfile();
    void renameProfile();
    void deleteProfile();
    void importFromInstance();
    void changeTargetVersion();
    void clearTargetVersion();

    QListView* m_list;
    QPushButton* m_duplicateButton;
    QPushButton* m_renameButton;
    QPushButton* m_deleteButton;
    QWidget* m_details;
    QLabel* m_name;
    QLabel* m_targetVersion;
    QPushButton* m_clearTargetButton;
    QLabel* m_usage;
    QTreeWidget* m_options;
};
