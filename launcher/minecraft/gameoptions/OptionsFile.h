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

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <optional>

#include "Result.h"

/*!
 * Minecraft's options.txt: one "key:value" entry per line, split on the first ':'.
 *
 * Everything that isn't touched is kept as it was (order, unknown keys, malformed lines),
 * so loading and saving a file without changes writes back the exact same lines.
 */
class OptionsFile {
   public:
    static OptionsFile parse(const QByteArray& data);
    /// a missing file is an empty options file, as that is what Minecraft starts with
    static Result<OptionsFile> load(const QString& path);

    QByteArray serialize() const;
    Result<> save(const QString& path) const;

    bool isEmpty() const { return m_lines.isEmpty(); }
    bool contains(const QString& key) const { return m_index.contains(key); }
    std::optional<QString> value(const QString& key) const;
    /// changes the value in place, or appends the entry if the key is new
    void set(const QString& key, const QString& value);
    void remove(const QString& key);

    /// keys of all entries, in file order
    QStringList keys() const;
    /// the effective value of every key
    QMap<QString, QString> values() const;

    /// the data version of the Minecraft version that wrote the file ("version" entry), if any
    std::optional<int> dataVersion() const;

   private:
    struct Line {
        QString key;    // empty for lines that aren't entries
        QString value;  // the raw line for lines that aren't entries
        bool isEntry = false;
    };

    void rebuildIndex();

    QList<Line> m_lines;
    bool m_hasByteOrderMark = false;
    // key -> index of the line holding its effective value; Minecraft uses the last one if a key is repeated
    QMap<QString, qsizetype> m_index;
};
