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
/// Saving or discarding ends the instance's sync session; closing the dialog keeps the changes to decide about later.
class GameOptionsReviewDialog : public QDialog {
    Q_OBJECT
   public:
    /// exec() result when the changes were discarded (accepted: saved, rejected: decide later)
    static constexpr int Discarded = 2;

    GameOptionsReviewDialog(QString instanceId,
                            const QString& instanceName,
                            QString profileId,
                            QList<GameOptionChange> changes,
                            GameOptionsCompat::ClientFormat client,
                            QWidget* parent = nullptr);
    ~GameOptionsReviewDialog() override;

    /// the dialog currently open for the instance, if any
    static GameOptionsReviewDialog* openFor(const QString& instanceId);

    void accept() override;

   private:
    void discard();
    void setAllChecked(bool checked);

    QString m_instanceId;
    QString m_profileId;
    QList<GameOptionChange> m_changes;
    GameOptionsCompat::ClientFormat m_client;
    QTreeWidget* m_list;
};
