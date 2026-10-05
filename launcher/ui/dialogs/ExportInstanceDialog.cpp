// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
 *  Copyright (c) 2023 Trial97 <alexandru.tripon97@gmail.com>
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "ExportInstanceDialog.h"
#include <BaseInstance.h>
#include <MMCZip.h>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QMessageBox>
#include "FileIgnoreProxy.h"
#include "QObjectPtr.h"
#include "archive/ExportToZipTask.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui_ExportInstanceDialog.h"

#include <FileSystem.h>
#include <icons/IconList.h>
#include <QDebug>
#include <QFileInfo>
#include <QPushButton>
#include <QSaveFile>
#include <QSortFilterProxyModel>
#include <QStack>
#include <functional>
#include "Application.h"
#include "InstanceList.h"
#include "SeparatorPrefixTree.h"
#include "tasks/ConcurrentTask.h"

namespace {
QString ignoreFileNameFor(BaseInstance* instance)
{
    return FS::PathCombine(instance->instanceRoot(), ".packignore");
}

void setupDefaultIgnores(FileIgnoreProxy* proxy, BaseInstance* instance)
{
    auto prefix = QDir(instance->instanceRoot()).relativeFilePath(instance->gameRoot());
    for (auto path : { "logs", "crash-reports", ".cache", ".fabric", ".quilt" }) {
        proxy->ignoreFilesWithPath().insert(FS::PathCombine(prefix, path));
    }
    proxy->ignoreFilesWithName().append({ ".DS_Store", "thumbs.db", "Thumbs.db", "gameoptions-session.json" });
    proxy->loadBlockedPathsFromFile(ignoreFileNameFor(instance));
}
}  // namespace

ExportInstanceDialog::ExportInstanceDialog(BaseInstance* instance, QWidget* parent)
    : QDialog(parent), m_ui(new Ui::ExportInstanceDialog), m_instance(instance)
{
    m_ui->setupUi(this);
    auto model = new QFileSystemModel(this);
    model->setIconProvider(&m_icons);
    auto root = instance->instanceRoot();
    m_proxyModel = new FileIgnoreProxy(root, this);
    m_proxyModel->setSourceModel(model);
    setupDefaultIgnores(m_proxyModel, instance);

    m_ui->treeView->setModel(m_proxyModel);
    m_ui->treeView->setRootIndex(m_proxyModel->mapFromSource(model->index(root)));
    m_ui->treeView->sortByColumn(0, Qt::AscendingOrder);

    connect(m_proxyModel, &QAbstractItemModel::rowsInserted, this, &ExportInstanceDialog::rowsInserted);

    model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Hidden);
    model->setRootPath(root);
    auto headerView = m_ui->treeView->header();
    headerView->setSectionResizeMode(QHeaderView::ResizeToContents);
    headerView->setSectionResizeMode(0, QHeaderView::Stretch);

    m_ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("OK"));
}

ExportInstanceDialog::~ExportInstanceDialog()
{
    delete m_ui;
}

/// Save icon to instance's folder is needed
void SaveIcon(BaseInstance* m_instance)
{
    auto iconKey = m_instance->iconKey();
    auto iconList = APPLICATION->icons();
    auto mmcIcon = iconList->icon(iconKey);
    if (!mmcIcon || mmcIcon->isBuiltIn()) {
        return;
    }
    auto path = mmcIcon->getFilePath();
    if (!path.isNull()) {
        QFileInfo inInfo(path);
        FS::copy(path, FS::PathCombine(m_instance->instanceRoot(), inInfo.fileName()))();
        return;
    }
    auto& image = mmcIcon->m_images[mmcIcon->type()];
    auto& icon = image.icon;
    auto sizes = icon.availableSizes();
    if (sizes.size() == 0) {
        return;
    }
    auto areaOf = [](QSize size) { return size.width() * size.height(); };
    QSize largest = sizes[0];
    // find variant with largest area
    for (auto size : sizes) {
        if (areaOf(largest) < areaOf(size)) {
            largest = size;
        }
    }
    auto pixmap = icon.pixmap(largest);
    pixmap.save(FS::PathCombine(m_instance->instanceRoot(), iconKey + ".png"));
}

void ExportInstanceDialog::doExport()
{
    auto name = FS::RemoveInvalidFilenameChars(m_instance->name());

    const QString output = QFileDialog::getSaveFileName(this, tr("Export %1").arg(m_instance->name()),
                                                        FS::PathCombine(QDir::homePath(), name + ".zip"), "Zip (*.zip)", nullptr);
    if (output.isEmpty()) {
        QDialog::done(QDialog::Rejected);
        return;
    }

    SaveIcon(m_instance);

    auto files = QFileInfoList();
    if (!MMCZip::collectFileListRecursively(m_instance->instanceRoot(), nullptr, &files,
                                            std::bind(&FileIgnoreProxy::filterFile, m_proxyModel, std::placeholders::_1))) {
        QMessageBox::warning(this, tr("Error"), tr("Unable to export instance"));
        QDialog::done(QDialog::Rejected);
        return;
    }

    auto task = makeShared<MMCZip::ExportToZipTask>(output, m_instance->instanceRoot(), files, "", true);

    connect(task.get(), &Task::failed, this,
            [this, output](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show(); });
    connect(task.get(), &Task::finished, this, [task] { task->deleteLater(); });

    ProgressDialog progress(this);
    progress.showSkipButton();
    auto result = progress.execWithTask(task.get());
    QDialog::done(result);
}

void ExportInstanceDialog::done(int result)
{
    m_proxyModel->saveBlockedPathsToFile(ignoreFileName());
    if (result == QDialog::Accepted) {
        doExport();
        return;
    }
    QDialog::done(result);
}

void ExportInstanceDialog::rowsInserted(QModelIndex parent, int top, int bottom)
{
    // WARNING: possible off-by-one?
    for (int i = top; i < bottom; i++) {
        auto node = m_proxyModel->index(i, 0, parent);
        if (m_proxyModel->shouldExpand(node)) {
            auto expNode = node.parent();
            if (!expNode.isValid()) {
                continue;
            }
            m_ui->treeView->expand(node);
        }
    }
}

QString ExportInstanceDialog::ignoreFileName()
{
    return ignoreFileNameFor(m_instance);
}

void exportInstancesToZips(const QStringList& instanceIds, QWidget* parent)
{
    if (instanceIds.isEmpty())
        return;

    const QString outputDir = QFileDialog::getExistingDirectory(
        parent, QObject::tr("Export %n instance(s) to", nullptr, static_cast<int>(instanceIds.size())), QDir::homePath());
    if (outputDir.isEmpty())
        return;

    // pick a unique output file for every instance, so that instances with the same name don't overwrite each other.
    // compared case insensitively, as "Foo.zip" and "foo.zip" are the same file on Windows and macOS
    QSet<QString> usedPaths;
    QStringList existing;
    QList<std::pair<QString, QString>> outputs;  // instance id, output file
    for (const auto& id : instanceIds) {
        // instances may have been removed while the dialog was open
        auto* instance = APPLICATION->instances()->getInstanceById(id);
        if (!instance)
            continue;
        auto baseName = FS::RemoveInvalidFilenameChars(instance->name());
        auto output = FS::PathCombine(outputDir, baseName + ".zip");
        for (int i = 2; usedPaths.contains(output.toLower()); i++) {
            output = FS::PathCombine(outputDir, QString("%1 (%2).zip").arg(baseName).arg(i));
        }
        usedPaths.insert(output.toLower());
        if (QFileInfo::exists(output))
            existing.append(QFileInfo(output).fileName());
        outputs.append({ id, output });
    }

    if (!existing.isEmpty()) {
        auto response = CustomMessageBox::selectable(parent, QObject::tr("Overwrite files?"),
                                                     QObject::tr("The following files already exist and will be overwritten:\n\n%1\n\n"
                                                                 "Do you want to continue?")
                                                         .arg(existing.join('\n')),
                                                     QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                            ->exec();
        if (response != QMessageBox::Yes)
            return;
    }

    auto task = makeShared<ConcurrentTask>(QObject::tr("Exporting instances"), 1);
    QStringList collectFailures;
    int exports = 0;
    for (const auto& [id, output] : outputs) {
        auto* instance = APPLICATION->instances()->getInstanceById(id);
        if (!instance)
            continue;
        SaveIcon(instance);

        FileIgnoreProxy proxy(instance->instanceRoot(), nullptr);
        setupDefaultIgnores(&proxy, instance);

        auto files = QFileInfoList();
        if (!MMCZip::collectFileListRecursively(instance->instanceRoot(), nullptr, &files,
                                                std::bind(&FileIgnoreProxy::filterFile, &proxy, std::placeholders::_1))) {
            collectFailures.append(instance->name());
            continue;
        }
        task->addTask(makeShared<MMCZip::ExportToZipTask>(output, instance->instanceRoot(), files, "", true));
        exports++;
    }

    if (!collectFailures.isEmpty()) {
        CustomMessageBox::selectable(parent, QObject::tr("Error"),
                                     QObject::tr("Unable to export the following instance(s):\n\n%1").arg(collectFailures.join('\n')),
                                     QMessageBox::Critical)
            ->show();
    }
    if (exports == 0)
        return;

    QObject::connect(task.get(), &Task::failed, parent, [parent](QString reason) {
        CustomMessageBox::selectable(parent, QObject::tr("Error"), reason, QMessageBox::Critical)->show();
    });

    ProgressDialog progress(parent);
    progress.showSkipButton();
    progress.execWithTask(task.get());
}
