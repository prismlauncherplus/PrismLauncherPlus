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

class MinecraftSettingsWidget;
class SettingsObject;

/// Edits the default settings of an instance group: they override the global settings for every instance in the group,
/// and are overridden by the settings of the instances themselves
class GroupSettingsDialog : public QDialog {
    Q_OBJECT

   public:
    GroupSettingsDialog(const QString& group, SettingsObject* settings, QWidget* parent = nullptr);

    void accept() override;

   private:
    SettingsObject* m_settings;
    MinecraftSettingsWidget* m_settingsWidget;
};
