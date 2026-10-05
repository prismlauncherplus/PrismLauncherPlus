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

class BaseInstance;
class QCheckBox;
class QDialogButtonBox;
class QLineEdit;
class QTreeWidget;

/// Dialog for renaming several instances at once using a name pattern and an optional find/replace
class BulkRenameDialog : public QDialog {
    Q_OBJECT

   public:
    explicit BulkRenameDialog(const QList<BaseInstance*>& instances, QWidget* parent = nullptr);

    /// the new names, in the same order as the instances passed to the constructor
    QStringList newNames() const;
    bool renameFolders() const;

   private:
    QString newNameFor(int index) const;
    void updatePreview();

    QList<BaseInstance*> m_instances;
    QLineEdit* m_pattern;
    QLineEdit* m_find;
    QLineEdit* m_replace;
    QCheckBox* m_renameFolders;
    QTreeWidget* m_preview;
    QDialogButtonBox* m_buttons;
};
