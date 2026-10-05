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
#include "GameOptionsProfilesPage.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "Application.h"
#include "InstanceList.h"
#include "meta/Index.h"
#include "meta/VersionList.h"
#include "minecraft/PackProfile.h"
#include "minecraft/gameoptions/GameOptionsProfileList.h"
#include "minecraft/gameoptions/GameOptionsSync.h"
#include "minecraft/gameoptions/OptionsFile.h"
#include "settings/SettingsObject.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/VersionSelectDialog.h"

GameOptionsProfilesPage::GameOptionsProfilesPage(QWidget* parent) : QWidget(parent)
{
    auto* profiles = APPLICATION->gameOptionsProfiles();
    auto* layout = new QVBoxLayout(this);

    auto* description = new QLabel(tr("Game options profiles keep Minecraft's own settings (video, controls, sound, ...) in sync between "
                                      "instances. Choose the profile to use in the Minecraft settings, for a group or for an "
                                      "instance. Options changed in game are saved to the profile when the game closes."),
                                   this);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto* columns = new QHBoxLayout();
    layout->addLayout(columns, 1);

    // profile list and actions
    auto* listColumn = new QVBoxLayout();
    m_list = new QListView(this);
    m_list->setModel(profiles);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    listColumn->addWidget(m_list, 1);

    auto* createButton = new QPushButton(tr("&New"), this);
    m_duplicateButton = new QPushButton(tr("D&uplicate"), this);
    m_renameButton = new QPushButton(tr("&Rename"), this);
    m_deleteButton = new QPushButton(tr("&Delete"), this);
    auto* importButton = new QPushButton(tr("&Import from Instance..."), this);
    importButton->setToolTip(tr("Copy the options of an instance into the selected profile, or into a new one if none is selected"));
    for (auto* button : { createButton, m_duplicateButton, m_renameButton, m_deleteButton, importButton }) {
        listColumn->addWidget(button);
    }
    columns->addLayout(listColumn, 1);

    // details of the selected profile
    m_details = new QWidget(this);
    auto* detailsLayout = new QVBoxLayout(m_details);
    detailsLayout->setContentsMargins(0, 0, 0, 0);
    auto* form = new QFormLayout();
    m_name = new QLabel(m_details);
    form->addRow(tr("Name:"), m_name);

    auto* versionRow = new QHBoxLayout();
    m_targetVersion = new QLabel(m_details);
    auto* changeTargetButton = new QPushButton(tr("&Change..."), m_details);
    m_clearTargetButton = new QPushButton(tr("C&lear"), m_details);
    versionRow->addWidget(m_targetVersion, 1);
    versionRow->addWidget(changeTargetButton);
    versionRow->addWidget(m_clearTargetButton);
    form->addRow(tr("Minecraft version:"), versionRow);

    m_usage = new QLabel(m_details);
    m_usage->setWordWrap(true);
    form->addRow(tr("Used by:"), m_usage);
    detailsLayout->addLayout(form);

    m_options = new QTreeWidget(m_details);
    m_options->setColumnCount(3);
    m_options->setHeaderLabels({ tr("Option"), tr("Value"), tr("Saved by") });
    m_options->headerItem()->setToolTip(2, tr("The data version of the Minecraft version that saved the value"));
    m_options->setRootIsDecorated(false);
    m_options->setSortingEnabled(true);
    m_options->sortByColumn(0, Qt::AscendingOrder);
    m_options->header()->setSectionResizeMode(QHeaderView::Interactive);
    detailsLayout->addWidget(m_options, 1);
    columns->addWidget(m_details, 2);

    connect(createButton, &QPushButton::clicked, this, &GameOptionsProfilesPage::createProfile);
    connect(m_duplicateButton, &QPushButton::clicked, this, &GameOptionsProfilesPage::duplicateProfile);
    connect(m_renameButton, &QPushButton::clicked, this, &GameOptionsProfilesPage::renameProfile);
    connect(m_deleteButton, &QPushButton::clicked, this, &GameOptionsProfilesPage::deleteProfile);
    connect(importButton, &QPushButton::clicked, this, &GameOptionsProfilesPage::importFromInstance);
    connect(changeTargetButton, &QPushButton::clicked, this, &GameOptionsProfilesPage::changeTargetVersion);
    connect(m_clearTargetButton, &QPushButton::clicked, this, &GameOptionsProfilesPage::clearTargetVersion);
    connect(m_list->selectionModel(), &QItemSelectionModel::currentChanged, this, &GameOptionsProfilesPage::updateDetails);
    // profiles also change when instances write their options back
    connect(profiles, &QAbstractItemModel::dataChanged, this, &GameOptionsProfilesPage::updateDetails);
    connect(profiles, &QAbstractItemModel::modelReset, this, &GameOptionsProfilesPage::updateDetails);
    connect(profiles, &QAbstractItemModel::rowsRemoved, this, &GameOptionsProfilesPage::updateDetails);

    if (profiles->rowCount() > 0) {
        m_list->setCurrentIndex(profiles->index(0));
    }
    updateDetails();
}

QString GameOptionsProfilesPage::Usage::describe() const
{
    QStringList parts;
    if (global) {
        parts << tr("the global settings");
    }
    if (!groups.isEmpty()) {
        parts << tr("%n group(s)", nullptr, static_cast<int>(groups.size()));
    }
    if (!instances.isEmpty()) {
        parts << tr("%n instance(s)", nullptr, static_cast<int>(instances.size()));
    }
    return parts.isEmpty() ? tr("nothing") : parts.join(", ");
}

GameOptionsProfilesPage::Usage GameOptionsProfilesPage::usageOf(const QString& profileId) const
{
    Usage usage;
    usage.global = APPLICATION->settings()->get("GameOptionsProfile").toString() == profileId;
    auto* instances = APPLICATION->instances();
    for (const auto& group : instances->getGroups()) {
        auto* settings = instances->groupSettings(group);
        if (settings && settings->get("OverrideGameOptionsProfile").toBool() &&
            settings->get("GameOptionsProfile").toString() == profileId) {
            usage.groups << group;
        }
    }
    for (int i = 0; i < instances->count(); i++) {
        auto* settings = instances->at(i)->settings();
        if (settings->get("OverrideGameOptionsProfile").toBool() && settings->get("GameOptionsProfile").toString() == profileId) {
            usage.instances << instances->at(i)->id();
        }
    }
    return usage;
}

QString GameOptionsProfilesPage::selectedId() const
{
    return m_list->currentIndex().data(GameOptionsProfileList::IdRole).toString();
}

void GameOptionsProfilesPage::select(const QString& profileId)
{
    auto* profiles = APPLICATION->gameOptionsProfiles();
    for (int row = 0; row < profiles->rowCount(); row++) {
        if (profiles->index(row).data(GameOptionsProfileList::IdRole).toString() == profileId) {
            m_list->setCurrentIndex(profiles->index(row));
            return;
        }
    }
}

void GameOptionsProfilesPage::updateDetails()
{
    const auto* profile = APPLICATION->gameOptionsProfiles()->profile(selectedId());
    m_details->setEnabled(profile != nullptr);
    m_duplicateButton->setEnabled(profile != nullptr);
    m_renameButton->setEnabled(profile != nullptr);
    m_deleteButton->setEnabled(profile != nullptr);
    m_options->clear();
    if (!profile) {
        m_name->clear();
        m_targetVersion->clear();
        m_usage->clear();
        return;
    }

    m_name->setText(profile->name);
    m_targetVersion->setText(profile->targetVersion.isEmpty() ? tr("Any") : profile->targetVersion);
    m_clearTargetButton->setEnabled(!profile->targetVersion.isEmpty());
    m_usage->setText(usageOf(profile->id).describe());

    m_options->setSortingEnabled(false);
    for (auto option = profile->options.begin(); option != profile->options.end(); ++option) {
        // options stored for several Minecraft versions get a row per version
        for (const auto& value : *option) {
            auto* item = new QTreeWidgetItem(
                m_options, { option.key(), value.value, value.dataVersion ? QString::number(*value.dataVersion) : tr("Unknown") });
            item->setToolTip(1, value.value);
        }
    }
    m_options->setSortingEnabled(true);
    m_options->resizeColumnToContents(0);
}

void GameOptionsProfilesPage::showError(const QString& title, const QString& error)
{
    CustomMessageBox::selectable(this, title, error, QMessageBox::Critical)->exec();
}

void GameOptionsProfilesPage::createProfile()
{
    bool ok = false;
    auto name = QInputDialog::getText(this, tr("New Game Options Profile"), tr("Name:"), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty()) {
        return;
    }
    auto id = APPLICATION->gameOptionsProfiles()->createProfile(name);
    if (!id) {
        showError(tr("Could not create the profile"), id.error());
        return;
    }
    select(*id);
}

void GameOptionsProfilesPage::duplicateProfile()
{
    const auto* profile = APPLICATION->gameOptionsProfiles()->profile(selectedId());
    if (!profile) {
        return;
    }
    bool ok = false;
    auto name = QInputDialog::getText(this, tr("Duplicate Game Options Profile"), tr("Name:"), QLineEdit::Normal,
                                      tr("Copy of %1").arg(profile->name), &ok)
                    .trimmed();
    if (!ok || name.isEmpty()) {
        return;
    }
    auto id = APPLICATION->gameOptionsProfiles()->duplicateProfile(selectedId(), name);
    if (!id) {
        showError(tr("Could not duplicate the profile"), id.error());
        return;
    }
    select(*id);
}

void GameOptionsProfilesPage::renameProfile()
{
    const auto* profile = APPLICATION->gameOptionsProfiles()->profile(selectedId());
    if (!profile) {
        return;
    }
    const auto id = profile->id;
    const auto targetVersion = profile->targetVersion;
    bool ok = false;
    auto name =
        QInputDialog::getText(this, tr("Rename Game Options Profile"), tr("Name:"), QLineEdit::Normal, profile->name, &ok).trimmed();
    if (!ok || name.isEmpty()) {
        return;
    }
    if (auto result = APPLICATION->gameOptionsProfiles()->setProfileInfo(id, name, targetVersion); !result) {
        showError(tr("Could not rename the profile"), result.error());
    }
}

void GameOptionsProfilesPage::deleteProfile()
{
    const auto* profile = APPLICATION->gameOptionsProfiles()->profile(selectedId());
    if (!profile) {
        return;
    }
    const auto id = profile->id;
    const auto usage = usageOf(id);
    auto question = tr("Are you sure you want to delete the game options profile \"%1\"?").arg(profile->name);
    if (!usage.isEmpty()) {
        question += "\n\n" + tr("It is used by %1, which will use no profile instead.").arg(usage.describe());
    }
    auto response = CustomMessageBox::selectable(this, tr("Delete Game Options Profile"), question, QMessageBox::Warning,
                                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                        ->exec();
    if (response != QMessageBox::Yes) {
        return;
    }

    // switch everything using it to "no profile", rather than silently falling back to another profile
    if (usage.global) {
        APPLICATION->settings()->set("GameOptionsProfile", QString());
    }
    for (const auto& group : usage.groups) {
        APPLICATION->instances()->groupSettings(group)->set("GameOptionsProfile", QString());
    }
    for (const auto& instanceId : usage.instances) {
        if (auto* instance = APPLICATION->instances()->getInstanceById(instanceId)) {
            instance->settings()->set("GameOptionsProfile", QString());
        }
    }
    if (auto result = APPLICATION->gameOptionsProfiles()->removeProfile(id); !result) {
        showError(tr("Could not delete the profile"), result.error());
    }
}

void GameOptionsProfilesPage::importFromInstance()
{
    auto* instances = APPLICATION->instances();
    QStringList names;
    QStringList ids;
    for (int i = 0; i < instances->count(); i++) {
        names << instances->at(i)->name();
        ids << instances->at(i)->id();
    }
    if (names.isEmpty()) {
        return;
    }
    bool ok = false;
    auto chosen = QInputDialog::getItem(this, tr("Import Game Options"), tr("Copy the game options of:"), names, 0, false, &ok);
    if (!ok) {
        return;
    }
    // look the instance up again, it may have been removed while the dialog was open
    auto* instance = instances->getInstanceById(ids.value(names.indexOf(chosen)));
    if (!instance) {
        return;
    }

    auto file = OptionsFile::load(GameOptionsSync::optionsPath(instance));
    if (!file || file->isEmpty()) {
        showError(tr("Could not import the game options"),
                  file ? tr("%1 has no game options yet. Launch it once to create them.").arg(instance->name()) : file.error());
        return;
    }
    const auto client = GameOptionsCompat::ClientFormat::detect(*file, GameOptionsSync::dataVersionFromJar(instance));
    const auto minecraftVersion = instance->getPackProfile()->getComponentVersion("net.minecraft");

    auto* profiles = APPLICATION->gameOptionsProfiles();
    auto profileId = selectedId();
    if (const auto* profile = profiles->profile(profileId)) {
        auto response = CustomMessageBox::selectable(this, tr("Import Game Options"),
                                                     tr("Copy the game options of \"%1\" into the profile \"%2\"? Options the profile "
                                                        "already has for this Minecraft version are replaced.")
                                                         .arg(instance->name(), profile->name),
                                                     QMessageBox::Question, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
                            ->exec();
        if (response != QMessageBox::Yes) {
            return;
        }
    } else {
        auto created = profiles->createProfile(instance->name(), minecraftVersion);
        if (!created) {
            showError(tr("Could not create the profile"), created.error());
            return;
        }
        profileId = *created;
    }

    const auto now = QDateTime::currentDateTimeUtc();
    auto result = profiles->modifyProfile(profileId, [&file, &client, &now, &minecraftVersion](GameOptionsProfile& profile) {
        profile.importFrom(*file, client, now);
        if (profile.targetVersion.isEmpty()) {
            profile.targetVersion = minecraftVersion;
        }
    });
    if (!result) {
        showError(tr("Could not import the game options"), result.error());
        return;
    }
    select(profileId);
}

void GameOptionsProfilesPage::changeTargetVersion()
{
    const auto* profile = APPLICATION->gameOptionsProfiles()->profile(selectedId());
    if (!profile) {
        return;
    }
    const auto id = profile->id;
    const auto name = profile->name;
    auto versions = APPLICATION->metadataIndex()->get("net.minecraft");
    VersionSelectDialog dialog(versions.get(), tr("Minecraft version of \"%1\"").arg(name), this);
    if (!profile->targetVersion.isEmpty()) {
        dialog.setCurrentVersion(profile->targetVersion);
    }
    if (dialog.exec() != QDialog::Accepted || !dialog.selectedVersion()) {
        return;
    }
    if (auto result = APPLICATION->gameOptionsProfiles()->setProfileInfo(id, name, dialog.selectedVersion()->descriptor()); !result) {
        showError(tr("Could not change the profile"), result.error());
    }
}

void GameOptionsProfilesPage::clearTargetVersion()
{
    const auto* profile = APPLICATION->gameOptionsProfiles()->profile(selectedId());
    if (!profile) {
        return;
    }
    if (auto result = APPLICATION->gameOptionsProfiles()->setProfileInfo(profile->id, profile->name, QString()); !result) {
        showError(tr("Could not change the profile"), result.error());
    }
}
