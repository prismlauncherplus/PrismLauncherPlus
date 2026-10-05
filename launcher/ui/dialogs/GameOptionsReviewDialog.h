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

#include <QDialog>
#include <QList>

#include "minecraft/gameoptions/GameOptionsCompat.h"
#include "minecraft/gameoptions/GameOptionsProfile.h"

class QTreeWidget;

/// Lets the user choose which of the game options changed in game are saved to the instance's game options profile.
/// Closing it without saving discards the changes.
class GameOptionsReviewDialog : public QDialog {
    Q_OBJECT
   public:
    GameOptionsReviewDialog(const QString& instanceName,
                            QString profileId,
                            QList<GameOptionChange> changes,
                            GameOptionsCompat::ClientFormat client,
                            QWidget* parent = nullptr);

    void accept() override;

   private:
    void setAllChecked(bool checked);

    QString m_profileId;
    QList<GameOptionChange> m_changes;
    GameOptionsCompat::ClientFormat m_client;
    QTreeWidget* m_list;
};
