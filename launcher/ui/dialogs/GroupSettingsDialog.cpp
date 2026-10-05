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
#include "GroupSettingsDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "settings/SettingsObject.h"
#include "ui/widgets/MinecraftSettingsWidget.h"

GroupSettingsDialog::GroupSettingsDialog(const QString& group, SettingsObject* settings, QWidget* parent)
    : QDialog(parent), m_settings(settings)
{
    setWindowTitle(tr("Group Settings - %1").arg(group));
    resize(800, 640);

    auto* layout = new QVBoxLayout(this);

    auto* description = new QLabel(tr("These settings override the global settings for every instance in the group <b>%1</b>. "
                                      "Settings that an instance overrides itself take priority over them.")
                                       .arg(group.toHtmlEscaped()),
                                   this);
    description->setWordWrap(true);
    layout->addWidget(description);

    m_settingsWidget = new MinecraftSettingsWidget(nullptr, settings, this);
    layout->addWidget(m_settingsWidget, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("OK"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void GroupSettingsDialog::accept()
{
    {
        // write the file once, instead of once per changed setting
        SettingsObject::Lock lock(m_settings);
        m_settingsWidget->saveSettings();
    }
    QDialog::accept();
}
