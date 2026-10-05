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

#include <launch/LaunchStep.h>
#include <optional>

#include "minecraft/gameoptions/GameOptionsSync.h"

/// Applies the instance's game options profile to options.txt, and writes the options changed in game back when it stops
class SyncGameOptions : public LaunchStep {
    Q_OBJECT
   public:
    explicit SyncGameOptions(LaunchTask* parent) : LaunchStep(parent) {}
    ~SyncGameOptions() override = default;

    void executeTask() override;
    bool canAbort() const override { return false; }

   public slots:
    /// runs when the launch ends in any way: the game stopped, crashed, was killed, or failed to start
    void finalize() override;

   private:
    void log(const QString& message, MessageLevel level);

    std::optional<GameOptionsSync::Session> m_session;
};
