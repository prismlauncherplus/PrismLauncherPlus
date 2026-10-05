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

#include <QStringList>
#include <functional>
#include <memory>

class Setting;
class SettingsObject;

/*!
 * The launcher settings that can be overridden by instance groups and instances.
 *
 * Each of them is registered as an override of a parent setting, enabled by a gate setting (e.g. "OverrideMemory").
 * Instances and groups register the exact same settings, so the same settings UI works for both of them.
 */
namespace InheritableSettings {
using ParentLookup = std::function<std::shared_ptr<Setting>(const QString& id)>;

/// settings shared by all instance types: game time, custom commands and console
void registerCommon(SettingsObject* settings, const ParentLookup& parent);

/// settings of Minecraft instances: Java, memory, window, native libraries, performance, ...
void registerMinecraft(SettingsObject* settings, const ParentLookup& parent);

/// the ids of all the gate settings, a group or instance overrides something if any of them is enabled
QStringList gateIds();
}  // namespace InheritableSettings
