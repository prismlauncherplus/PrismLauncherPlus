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
#include "GameOptionsSync.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

#include "Application.h"
#include "FileSystem.h"
#include "GameOptionsMerger.h"
#include "GameOptionsProfileList.h"
#include "InstanceList.h"
#include "OptionsFile.h"
#include "archive/ArchiveReader.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "ui/dialogs/GameOptionsReviewDialog.h"

#include <QApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace GameOptionsSync {

namespace {
constexpr int sessionFormatVersion = 1;

QString sessionPath(MinecraftInstance* instance)
{
    return FS::PathCombine(instance->instanceRoot(), "gameoptions-session.json");
}

void removeSessionFile(MinecraftInstance* instance)
{
    QFile::remove(sessionPath(instance));
}

bool isProcessRunning(qint64 pid)
{
    if (pid <= 0) {
        return false;
    }
#ifdef Q_OS_WIN
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!process) {
        return false;
    }
    DWORD exitCode = 0;
    bool running = GetExitCodeProcess(process, &exitCode) && exitCode == STILL_ACTIVE;
    CloseHandle(process);
    return running;
#else
    return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#endif
}

/// when the process started, where the system tells
std::optional<QDateTime> processStartTime(qint64 pid)
{
#if defined(Q_OS_WIN)
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!process) {
        return std::nullopt;
    }
    FILETIME creation, exit, kernel, user;
    bool ok = GetProcessTimes(process, &creation, &exit, &kernel, &user);
    CloseHandle(process);
    if (!ok) {
        return std::nullopt;
    }
    // 100 ns intervals since 1601
    const auto ticks = (static_cast<qint64>(creation.dwHighDateTime) << 32) | creation.dwLowDateTime;
    return QDateTime::fromMSecsSinceEpoch((ticks - 116444736000000000LL) / 10000, QTimeZone::UTC);
#elif defined(Q_OS_LINUX)
    // field 22 of /proc/<pid>/stat is the start time in clock ticks after boot; the fields before it can contain spaces
    QFile stat(QString("/proc/%1/stat").arg(pid));
    QFile systemStat("/proc/stat");
    if (!stat.open(QIODevice::ReadOnly) || !systemStat.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const auto fields = QString::fromLatin1(stat.readAll()).section(')', -1).split(' ', Qt::SkipEmptyParts);
    qint64 bootTime = -1;
    for (const auto& line : QString::fromLatin1(systemStat.readAll()).split('\n')) {
        if (line.startsWith("btime ")) {
            bootTime = line.mid(6).trimmed().toLongLong();
        }
    }
    // fields after the command name start with field 3 (state)
    if (fields.size() < 20 || bootTime < 0) {
        return std::nullopt;
    }
    const auto ticksPerSecond = sysconf(_SC_CLK_TCK);
    return QDateTime::fromSecsSinceEpoch(bootTime + fields[19].toLongLong() / ticksPerSecond, QTimeZone::UTC);
#else
    Q_UNUSED(pid);
    return std::nullopt;
#endif
}

/// whether the game of the session is still running, and not just another process with the same id
bool isGameRunning(const Session& session)
{
    if (!session.gamePid || !isProcessRunning(*session.gamePid)) {
        return false;
    }
    auto started = processStartTime(*session.gamePid);
    if (started && session.gameStarted) {
        return std::abs(started->secsTo(*session.gameStarted)) <= 120;
    }
    return true;
}

QString keybindFormatName(GameOptionsCompat::KeybindFormat format)
{
    switch (format) {
        case GameOptionsCompat::KeybindFormat::Codes:
            return "codes";
        case GameOptionsCompat::KeybindFormat::Names:
            return "names";
        case GameOptionsCompat::KeybindFormat::Unknown:
            break;
    }
    return "unknown";
}
}  // namespace

std::optional<Session> loadSession(MinecraftInstance* instance)
{
    if (!QFileInfo::exists(sessionPath(instance))) {
        return std::nullopt;
    }
    auto data = FS::read(sessionPath(instance));
    auto document = data ? QJsonDocument::fromJson(*data) : QJsonDocument();
    auto session = Session::fromJson(document.object());
    if (!session) {
        qWarning() << "Ignoring the broken game options session of" << instance->id() << ":" << session.error();
        removeSessionFile(instance);
        return std::nullopt;
    }
    return *session;
}

QJsonObject Session::toJson() const
{
    QJsonObject snapshotObject;
    for (auto iter = snapshot.begin(); iter != snapshot.end(); ++iter) {
        snapshotObject.insert(iter.key(), iter.value());
    }
    QJsonObject json{ { "formatVersion", sessionFormatVersion },
                      { "profileId", profileId },
                      { "keybinds", keybindFormatName(client.keybinds) },
                      { "snapshot", snapshotObject } };
    if (client.dataVersion) {
        json.insert("dataVersion", *client.dataVersion);
    }
    if (gamePid) {
        json.insert("gamePid", *gamePid);
    }
    if (gameStarted) {
        json.insert("gameStarted", gameStarted->toString(Qt::ISODate));
    }
    return json;
}

Result<Session> Session::fromJson(const QJsonObject& json)
{
    if (json.value("formatVersion").toInt() != sessionFormatVersion || json.value("profileId").toString().isEmpty()) {
        return std::unexpected(QString("invalid game options session"));
    }
    Session session;
    session.profileId = json.value("profileId").toString();
    if (json.contains("dataVersion")) {
        session.client.dataVersion = json.value("dataVersion").toInt();
    }
    auto keybinds = json.value("keybinds").toString();
    session.client.keybinds = keybinds == "codes"   ? GameOptionsCompat::KeybindFormat::Codes
                              : keybinds == "names" ? GameOptionsCompat::KeybindFormat::Names
                                                    : GameOptionsCompat::KeybindFormat::Unknown;
    if (json.contains("gamePid")) {
        session.gamePid = json.value("gamePid").toInteger();
    }
    if (json.contains("gameStarted")) {
        session.gameStarted = QDateTime::fromString(json.value("gameStarted").toString(), Qt::ISODate);
    }
    const auto snapshotObject = json.value("snapshot").toObject();
    for (auto iter = snapshotObject.begin(); iter != snapshotObject.end(); ++iter) {
        session.snapshot.insert(iter.key(), iter.value().toString());
    }
    return session;
}

QString optionsPath(MinecraftInstance* instance)
{
    return FS::PathCombine(instance->gameRoot(), "options.txt");
}

std::optional<int> dataVersionFromJar(MinecraftInstance* instance)
{
    auto profile = instance->getPackProfile()->getProfile();
    auto mainJar = profile ? profile->getMainJar() : nullptr;
    if (!mainJar) {
        return std::nullopt;
    }
    QStringList jars, nativeJars, nativeJars32, nativeJars64;
    mainJar->getApplicableFiles(instance->runtimeContext(), jars, nativeJars, nativeJars32, nativeJars64, instance->getLocalLibraryPath());
    if (jars.isEmpty() || !QFileInfo::exists(jars.first())) {
        return std::nullopt;
    }

    QByteArray versionJson;
    MMCZip::ArchiveReader jar(jars.first());
    jar.parse([&versionJson](MMCZip::ArchiveReader::File* file, bool& stop) {
        if (file->filename() == "version.json") {
            versionJson = file->readAll();
            stop = true;
            return true;
        }
        return file->skip();
    });
    auto worldVersion = QJsonDocument::fromJson(versionJson).object().value("world_version");
    return worldVersion.isDouble() ? std::optional<int>(worldVersion.toInt()) : std::nullopt;
}

std::optional<Session> start(MinecraftInstance* instance, const Logger& log)
{
    // the changes of an earlier session that were never written back come first
    if (auto leftover = loadSession(instance)) {
        const auto* leftoverProfile = APPLICATION->gameOptionsProfiles()->profile(leftover->profileId);
        const auto leftoverName = leftoverProfile ? leftoverProfile->name : leftover->profileId;
        if (isGameRunning(*leftover)) {
            // the game of an earlier launch outlived the launcher; applying the profile would mix into its options
            log(QObject::tr("The game of an earlier launch of this instance is still running, so the game options profile is not "
                            "applied. Changed options are saved to \"%1\" afterwards.")
                    .arg(leftoverName),
                MessageLevel::Warning);
            return leftover;
        }
        leftover->gamePid.reset();
        leftover->gameStarted.reset();
        if (finish(instance, *leftover, log, ReviewMode::Modal) == FinishResult::Pending) {
            // finish() may have changed the session file (review), so continue from that
            auto pending = loadSession(instance).value_or(*leftover);
            pending.gamePid.reset();
            pending.gameStarted.reset();
            // applying the profile now would overwrite those changes, so keep collecting them in the earlier session
            log(QObject::tr("The game options changed the last time are not saved to the profile \"%1\" yet, so the profile is not "
                            "applied this time.")
                    .arg(leftoverName),
                MessageLevel::Warning);
            saveSession(instance, pending);
            return pending;
        }
    }

    const auto profileId = instance->settings()->get("GameOptionsProfile").toString();
    if (profileId.isEmpty()) {
        return std::nullopt;
    }
    const auto* profile = APPLICATION->gameOptionsProfiles()->profile(profileId);
    if (!profile) {
        log(QObject::tr("The game options profile of this instance no longer exists, so its game options are not shared."),
            MessageLevel::Warning);
        return std::nullopt;
    }

    const auto path = optionsPath(instance);
    if (QFileInfo(path).isSymLink()) {
        // merging into it would change the options of every instance sharing the file
        log(QObject::tr("options.txt is a link to another file, so the game options profile \"%1\" is not used.").arg(profile->name),
            MessageLevel::Warning);
        return std::nullopt;
    }
    auto file = OptionsFile::load(path);
    if (!file) {
        log(QObject::tr("Could not read options.txt, so the game options profile is not used: %1").arg(file.error()),
            MessageLevel::Warning);
        return std::nullopt;
    }

    const auto fallbackDataVersion = file->dataVersion() ? std::nullopt : dataVersionFromJar(instance);
    const auto client = GameOptionsCompat::ClientFormat::detect(*file, fallbackDataVersion);

    Session session{ profileId, client, {} };
    if (file->isEmpty() && !client.dataVersion) {
        // Without knowing the version, Minecraft would treat the options as written by a very old version and "upgrade" them.
        // The game creates the file itself this time, and its options are added to the profile when it stops.
        log(QObject::tr("This Minecraft version can't be detected, so the game options profile \"%1\" is applied from the next "
                        "launch on.")
                .arg(profile->name),
            MessageLevel::Launcher);
    } else {
        auto result = GameOptionsMerger::apply(*profile, *file, client);
        if (result.file.serialize() != file->serialize()) {
            // the first time a profile changes the instance's own options, keep them, as they can't be restored otherwise
            int replaced = 0;
            for (const auto& key : result.applied) {
                if (auto before = file->value(key); before && *before != result.file.value(key)) {
                    replaced++;
                }
            }
            // whenever a profile (other than the one used last time) replaces options, keep the previous ones
            const bool otherProfile = instance->settings()->get("GameOptionsLastProfile").toString() != profileId;
            if (replaced > 0 && otherProfile) {
                const auto backup = path + ".before-profile-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
                if (!QFile::copy(path, backup)) {
                    log(QObject::tr("Could not back up options.txt, so the game options profile is not applied."), MessageLevel::Warning);
                    return std::nullopt;
                }
                log(QObject::tr("The profile replaced %n option(s) of this instance, the previous options are kept in %1.", nullptr,
                                replaced)
                        .arg(QFileInfo(backup).fileName()),
                    MessageLevel::Launcher);
            }
            if (auto saved = result.file.save(path); !saved) {
                log(QObject::tr("Could not write options.txt, so the game options profile is not used: %1").arg(saved.error()),
                    MessageLevel::Warning);
                return std::nullopt;
            }
        }
        session.snapshot = result.snapshot;
        instance->settings()->set("GameOptionsLastProfile", profileId);
        auto message =
            QObject::tr("Applied %n option(s) of the game options profile \"%1\".", nullptr, static_cast<int>(result.applied.size()))
                .arg(profile->name);
        if (!result.skipped.isEmpty()) {
            message += ' ' + QObject::tr("%n option(s) are only stored for other Minecraft versions.", nullptr,
                                         static_cast<int>(result.skipped.size()));
        }
        log(message, MessageLevel::Launcher);
    }

    saveSession(instance, session);
    return session;
}

void saveSession(MinecraftInstance* instance, const Session& session)
{
    if (auto saved = FS::write(sessionPath(instance), QJsonDocument(session.toJson()).toJson()); !saved) {
        // only needed to recover from a crash, so the sync still works without it
        qWarning() << "Could not save the game options session:" << saved.error();
    }
}

void reviewed(const QString& instanceId, const QList<GameOptionChange>& decided)
{
    auto* instance = APPLICATION->instances()->getInstanceById(instanceId);
    if (!instance) {
        return;
    }
    auto session = loadSession(instance);
    if (!session) {
        return;
    }
    // saved or discarded, either way the user decided about these values
    for (const auto& change : decided) {
        session->snapshot.insert(change.key, change.newValue);
    }
    // a running game (e.g. launched again before reviewing) still uses the session, and options changed since then still count
    if (!instance->isRunning()) {
        auto file = OptionsFile::load(optionsPath(instance));
        if (file && GameOptionsMerger::collectChanges(session->snapshot, *file).isEmpty()) {
            removeSessionFile(instance);
            return;
        }
    }
    saveSession(instance, *session);
}

FinishResult finish(MinecraftInstance* instance, const Session& fallback, const Logger& log, ReviewMode reviewMode)
{
    // the session file is what counts: reviewing changes updates it
    const auto stored = loadSession(instance);
    const Session& session = stored ? *stored : fallback;

    if (auto* review = GameOptionsReviewDialog::openFor(instance->id())) {
        // the user hasn't decided about the earlier changes yet; show everything that changed by now
        if (auto file = OptionsFile::load(optionsPath(instance))) {
            review->setChanges(GameOptionsMerger::collectChanges(session.snapshot, *file),
                               GameOptionsCompat::ClientFormat::detect(*file, session.client.dataVersion));
        }
        review->raise();
        review->activateWindow();
        return FinishResult::Pending;
    }

    auto* profiles = APPLICATION->gameOptionsProfiles();
    if (!profiles->profile(session.profileId)) {
        // deleted while the game was running; there is nothing to save the changes to anymore
        log(QObject::tr("The game options profile no longer exists, so the changed game options are not saved."), MessageLevel::Warning);
        removeSessionFile(instance);
        return FinishResult::Done;
    }

    auto file = OptionsFile::load(optionsPath(instance));
    if (!file) {
        // the session file stays, so it is tried again later
        log(QObject::tr("Could not read options.txt to save the changed game options: %1").arg(file.error()), MessageLevel::Warning);
        return FinishResult::Pending;
    }
    // the game may differ from what was detected before it started (e.g. there was no options.txt), so ask what it wrote
    const auto client = GameOptionsCompat::ClientFormat::detect(*file, session.client.dataVersion);
    const auto changes = GameOptionsMerger::collectChanges(session.snapshot, *file);
    if (changes.isEmpty()) {
        removeSessionFile(instance);
        return FinishResult::Done;
    }

    if (instance->settings()->get("GameOptionsReviewChanges").toBool()) {
        // the session file stays until the user saves or discards the changes, so closing the dialog (or the launcher) loses nothing
        if (reviewMode == ReviewMode::Modal) {
            GameOptionsReviewDialog dialog(instance->id(), instance->name(), session.profileId, changes, client,
                                           QApplication::activeWindow());
            return dialog.exec() == QDialog::Rejected ? FinishResult::Pending : FinishResult::Done;
        }
        auto* dialog = new GameOptionsReviewDialog(instance->id(), instance->name(), session.profileId, changes, client);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        return FinishResult::Pending;
    }

    if (auto result = profiles->applyChanges(session.profileId, changes, client); !result) {
        // keep the session, so the changes aren't lost
        log(QObject::tr("Could not save the changed game options to the profile, trying again later: %1").arg(result.error()),
            MessageLevel::Warning);
        return FinishResult::Pending;
    }
    removeSessionFile(instance);
    log(QObject::tr("Saved %n changed game option(s) to the profile \"%1\".", nullptr, static_cast<int>(changes.size()))
            .arg(profiles->profile(session.profileId)->name),
        MessageLevel::Launcher);
    return FinishResult::Done;
}

void recoverSessions(InstanceList* instances)
{
    for (int i = 0; i < instances->count(); i++) {
        auto* instance = instances->at(i);
        auto session = loadSession(instance);
        if (!session) {
            continue;
        }
        if (isGameRunning(*session)) {
            // the game outlived the launcher and is still running, its changes are collected the next time it is launched
            qInfo() << "Not saving the game options of" << instance->id() << "yet, its game may still be running";
            continue;
        }
        qInfo() << "Saving the game options of" << instance->id() << "left over from the last time the launcher ran";
        finish(instance, *session, [](const QString& message, MessageLevel) { qInfo() << message; }, ReviewMode::NonModal);
    }
}

}  // namespace GameOptionsSync
