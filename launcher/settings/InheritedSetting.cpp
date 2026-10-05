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

#include "InheritedSetting.h"

#include "SettingsObject.h"

InheritedSetting::InheritedSetting(std::shared_ptr<Setting> fallback, Lookup lookup)
    : Setting(fallback->configKeys(), QVariant()), m_fallback(std::move(fallback)), m_lookup(std::move(lookup))
{}

std::shared_ptr<Setting> InheritedSetting::target() const
{
    if (auto* parent = m_lookup ? m_lookup() : nullptr) {
        if (auto setting = parent->getSetting(id())) {
            return setting;
        }
    }
    return m_fallback;
}

QVariant InheritedSetting::defValue() const
{
    return target()->defValue();
}

QVariant InheritedSetting::get() const
{
    return target()->get();
}

void InheritedSetting::set(QVariant value)
{
    target()->set(value);
}

void InheritedSetting::reset()
{
    target()->reset();
}
