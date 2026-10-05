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
#include "GameOptionsReviewDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "Application.h"
#include "minecraft/gameoptions/GameOptionsProfileList.h"
#include "ui/dialogs/CustomMessageBox.h"

GameOptionsReviewDialog::GameOptionsReviewDialog(const QString& instanceName,
                                                 QString profileId,
                                                 QList<GameOptionChange> changes,
                                                 GameOptionsCompat::ClientFormat client,
                                                 QWidget* parent)
    : QDialog(parent), m_profileId(std::move(profileId)), m_changes(std::move(changes)), m_client(client)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Changed Game Options - %1").arg(instanceName));
    resize(640, 480);

    const auto* profile = APPLICATION->gameOptionsProfiles()->profile(m_profileId);
    const auto profileName = profile ? profile->name : m_profileId;

    auto* layout = new QVBoxLayout(this);
    auto* description = new QLabel(tr("These game options of <b>%1</b> changed while playing. Choose the ones to save to the game "
                                      "options profile <b>%2</b>, to use them in the other instances sharing it.")
                                       .arg(instanceName.toHtmlEscaped(), profileName.toHtmlEscaped()),
                                   this);
    description->setWordWrap(true);
    layout->addWidget(description);

    m_list = new QTreeWidget(this);
    m_list->setColumnCount(3);
    m_list->setHeaderLabels({ tr("Option"), tr("Before"), tr("After") });
    m_list->setRootIsDecorated(false);
    m_list->header()->setSectionResizeMode(QHeaderView::Stretch);
    // changed options first; new ones (options the profile didn't have yet) after them
    QList<QTreeWidgetItem*> newItems;
    for (qsizetype i = 0; i < m_changes.size(); i++) {
        const auto& change = m_changes[i];
        auto* item = new QTreeWidgetItem({ change.key, change.oldValue.value_or(tr("(new)")), change.newValue });
        item->setCheckState(0, Qt::Checked);
        item->setData(0, Qt::UserRole, static_cast<int>(i));
        if (change.oldValue) {
            m_list->addTopLevelItem(item);
        } else {
            newItems.append(item);
        }
    }
    m_list->addTopLevelItems(newItems);
    layout->addWidget(m_list, 1);

    auto* selectionButtons = new QHBoxLayout();
    auto* selectAll = new QPushButton(tr("Select &All"), this);
    auto* selectNone = new QPushButton(tr("Select &None"), this);
    connect(selectAll, &QPushButton::clicked, this, [this] { setAllChecked(true); });
    connect(selectNone, &QPushButton::clicked, this, [this] { setAllChecked(false); });
    selectionButtons->addWidget(selectAll);
    selectionButtons->addWidget(selectNone);
    selectionButtons->addStretch();
    layout->addLayout(selectionButtons);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Discard, this);
    buttons->button(QDialogButtonBox::Save)->setText(tr("&Save Selected"));
    buttons->button(QDialogButtonBox::Discard)->setText(tr("&Discard"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons->button(QDialogButtonBox::Discard), &QPushButton::clicked, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void GameOptionsReviewDialog::setAllChecked(bool checked)
{
    for (int i = 0; i < m_list->topLevelItemCount(); i++) {
        m_list->topLevelItem(i)->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
    }
}

void GameOptionsReviewDialog::accept()
{
    QList<GameOptionChange> selected;
    for (int i = 0; i < m_list->topLevelItemCount(); i++) {
        auto* item = m_list->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked) {
            selected.append(m_changes[item->data(0, Qt::UserRole).toInt()]);
        }
    }
    if (auto result = APPLICATION->gameOptionsProfiles()->applyChanges(m_profileId, selected, m_client); !result) {
        CustomMessageBox::selectable(this, tr("Could not save the game options"), result.error(), QMessageBox::Critical)->exec();
        return;
    }
    QDialog::accept();
}
