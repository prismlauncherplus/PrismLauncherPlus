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

#include <functional>
#include <memory>

#include "Setting.h"

class SettingsObject;

/*!
 * A setting that stores nothing itself and forwards everything to the setting it currently inherits from:
 * the setting with the same id in the settings object returned by the lookup (e.g. the settings of the
 * instance's group) if there is one, otherwise the fallback (e.g. the global setting).
 *
 * The lookup is evaluated on every access, so the inherited setting follows changes like moving an instance to another group.
 */
class InheritedSetting : public Setting {
    Q_OBJECT
   public:
    using Lookup = std::function<SettingsObject*()>;

    InheritedSetting(std::shared_ptr<Setting> fallback, Lookup lookup);

    QVariant defValue() const override;
    QVariant get() const override;
    void set(QVariant value) override;
    void reset() override;

   private:
    std::shared_ptr<Setting> target() const;

    std::shared_ptr<Setting> m_fallback;
    Lookup m_lookup;
};
