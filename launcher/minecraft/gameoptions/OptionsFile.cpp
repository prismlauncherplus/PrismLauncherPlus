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
#include "OptionsFile.h"

#include <QFileInfo>

#include "FileSystem.h"

OptionsFile OptionsFile::parse(const QByteArray& data)
{
    OptionsFile file;
    // keep a byte order mark when writing the file again; it must not become part of the first key
    file.m_hasByteOrderMark = data.startsWith("\xEF\xBB\xBF");
    auto text = QString::fromUtf8(file.m_hasByteOrderMark ? data.mid(3) : data);
    if (text.endsWith('\n')) {
        text.chop(1);
    }
    if (text.isEmpty()) {
        return file;
    }
    for (auto line : text.split('\n')) {
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        auto separator = line.indexOf(':');
        if (separator <= 0) {
            file.m_lines.append({ QString(), line, false });
            continue;
        }
        file.m_lines.append({ line.left(separator), line.mid(separator + 1), true });
    }
    file.rebuildIndex();
    return file;
}

Result<OptionsFile> OptionsFile::load(const QString& path)
{
    if (!QFileInfo::exists(path)) {
        return OptionsFile();
    }
    auto data = FS::read(path);
    if (!data) {
        return std::unexpected(data.error());
    }
    return parse(*data);
}

QByteArray OptionsFile::serialize() const
{
    QString text;
    if (m_hasByteOrderMark) {
        text += QChar(0xFEFF);
    }
    for (const auto& line : m_lines) {
        if (line.isEntry) {
            text += line.key + ':' + line.value;
        } else {
            text += line.value;
        }
        text += '\n';
    }
    return text.toUtf8();
}

Result<> OptionsFile::save(const QString& path) const
{
    return FS::write(path, serialize());
}

std::optional<QString> OptionsFile::value(const QString& key) const
{
    auto iter = m_index.find(key);
    if (iter == m_index.end()) {
        return std::nullopt;
    }
    return m_lines[*iter].value;
}

void OptionsFile::set(const QString& key, const QString& value)
{
    auto iter = m_index.find(key);
    if (iter != m_index.end()) {
        m_lines[*iter].value = value;
        return;
    }
    m_lines.append({ key, value, true });
    m_index.insert(key, m_lines.size() - 1);
}

void OptionsFile::remove(const QString& key)
{
    if (!m_index.contains(key)) {
        return;
    }
    m_lines.removeIf([&key](const Line& line) { return line.isEntry && line.key == key; });
    rebuildIndex();
}

QStringList OptionsFile::keys() const
{
    QStringList keys;
    for (const auto& line : m_lines) {
        if (line.isEntry && !keys.contains(line.key)) {
            keys.append(line.key);
        }
    }
    return keys;
}

QMap<QString, QString> OptionsFile::values() const
{
    QMap<QString, QString> values;
    for (auto iter = m_index.begin(); iter != m_index.end(); ++iter) {
        values.insert(iter.key(), m_lines[iter.value()].value);
    }
    return values;
}

std::optional<int> OptionsFile::dataVersion() const
{
    auto version = value("version");
    if (!version) {
        return std::nullopt;
    }
    bool ok = false;
    int result = version->trimmed().toInt(&ok);
    return ok ? std::optional<int>(result) : std::nullopt;
}

void OptionsFile::rebuildIndex()
{
    m_index.clear();
    for (qsizetype i = 0; i < m_lines.size(); ++i) {
        if (m_lines[i].isEntry) {
            m_index.insert(m_lines[i].key, i);
        }
    }
}
