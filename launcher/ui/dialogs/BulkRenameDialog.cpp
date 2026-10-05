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

#include "BulkRenameDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "Application.h"
#include "settings/SettingsObject.h"

BulkRenameDialog::BulkRenameDialog(const QStringList& currentNames, QWidget* parent) : QDialog(parent), m_currentNames(currentNames)
{
    setWindowTitle(tr("Rename %n instance(s)", nullptr, static_cast<int>(currentNames.size())));
    resize(560, 440);

    auto layout = new QVBoxLayout(this);

    auto form = new QFormLayout();
    m_pattern = new QLineEdit(QStringLiteral("{name}"), this);
    form->addRow(tr("&Name pattern:"), m_pattern);
    m_find = new QLineEdit(this);
    form->addRow(tr("&Find:"), m_find);
    m_replace = new QLineEdit(this);
    form->addRow(tr("&Replace with:"), m_replace);
    layout->addLayout(form);

    auto help = new QLabel(tr("<b>{name}</b> is replaced with the current instance name and <b>{n}</b> with the position of the instance "
                              "in the list below (1, 2, 3, ...). Find and replace is applied afterwards."),
                           this);
    help->setWordWrap(true);
    layout->addWidget(help);

    m_preview = new QTreeWidget(this);
    m_preview->setColumnCount(2);
    m_preview->setHeaderLabels({ tr("Current name"), tr("New name") });
    m_preview->setRootIsDecorated(false);
    m_preview->setSelectionMode(QAbstractItemView::NoSelection);
    m_preview->header()->setSectionResizeMode(QHeaderView::Stretch);
    for (const auto& name : m_currentNames) {
        new QTreeWidgetItem(m_preview, { name, name });
    }
    layout->addWidget(m_preview);

    m_renameFolders = new QCheckBox(tr("Also rename the instance &folders"), this);
    m_renameFolders->setChecked(APPLICATION->settings()->get("InstRenamingMode").toString() == "PhysicalDir");
    layout->addWidget(m_renameFolders);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttons->button(QDialogButtonBox::Ok)->setText(tr("Rename"));
    m_buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(m_buttons);

    connect(m_pattern, &QLineEdit::textChanged, this, &BulkRenameDialog::updatePreview);
    connect(m_find, &QLineEdit::textChanged, this, &BulkRenameDialog::updatePreview);
    connect(m_replace, &QLineEdit::textChanged, this, &BulkRenameDialog::updatePreview);

    m_pattern->setFocus();
    m_pattern->selectAll();
    updatePreview();
}

QString BulkRenameDialog::newNameFor(int index) const
{
    // replace both placeholders in one pass, so a "{n}" in an instance name stays as it is
    static const QRegularExpression placeholders(QStringLiteral("\\{name\\}|\\{n\\}"));
    const QString pattern = m_pattern->text();
    QString name;
    qsizetype last = 0;
    for (auto match = placeholders.globalMatch(pattern); match.hasNext();) {
        auto placeholder = match.next();
        name += pattern.mid(last, placeholder.capturedStart() - last);
        name += placeholder.captured() == QStringLiteral("{name}") ? m_currentNames[index] : QString::number(index + 1);
        last = placeholder.capturedEnd();
    }
    name += pattern.mid(last);
    if (!m_find->text().isEmpty()) {
        name.replace(m_find->text(), m_replace->text());
    }
    name = name.trimmed();
    // same limit as renaming a single instance
    name.truncate(128);
    return name;
}

QStringList BulkRenameDialog::newNames() const
{
    QStringList names;
    for (int i = 0; i < m_currentNames.size(); i++) {
        names.append(newNameFor(i));
    }
    return names;
}

bool BulkRenameDialog::renameFolders() const
{
    return m_renameFolders->isChecked();
}

void BulkRenameDialog::updatePreview()
{
    bool valid = true;
    bool changed = false;
    for (int i = 0; i < m_currentNames.size(); i++) {
        auto name = newNameFor(i);
        auto item = m_preview->topLevelItem(i);
        if (name.isEmpty()) {
            valid = false;
            item->setText(1, tr("(empty name)"));
        } else {
            item->setText(1, name);
        }
        changed |= name != m_currentNames[i];
    }
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(valid && changed);
}
