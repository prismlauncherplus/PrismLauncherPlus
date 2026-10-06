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
#include "SyncGameOptions.h"

#include <launch/LaunchTask.h>

void SyncGameOptions::executeTask()
{
    m_session = GameOptionsSync::start(m_parent->instance(), [this](const QString& message, MessageLevel level) { log(message, level); });
    if (m_session) {
        // remember the game's process, so a restarted launcher doesn't collect the changes while the game still runs
        connect(m_parent, &LaunchTask::pidChanged, this, [this](qint64 pid) {
            if (m_session && pid > 0) {
                m_session->gamePid = pid;
                m_session->gameStarted = QDateTime::currentDateTimeUtc();
                // update the stored session, which a review may have changed since it started
                auto stored = GameOptionsSync::loadSession(m_parent->instance()).value_or(*m_session);
                stored.gamePid = m_session->gamePid;
                stored.gameStarted = m_session->gameStarted;
                GameOptionsSync::saveSession(m_parent->instance(), stored);
            }
        });
    }
    emitSucceeded();
}

void SyncGameOptions::finalize()
{
    if (!m_session) {
        return;
    }
    GameOptionsSync::finish(
        m_parent->instance(), *m_session, [this](const QString& message, MessageLevel level) { log(message, level); },
        GameOptionsSync::ReviewMode::NonModal);
    m_session.reset();
}

void SyncGameOptions::log(const QString& message, MessageLevel level)
{
    emit logLine(message, level);
}
