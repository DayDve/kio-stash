/*
 *   SPDX-FileCopyrightText: 2016 Arnav Dhamija <arnav.dhamija@gmail.com>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef STASHNOTIFIER_H
#define STASHNOTIFIER_H

#include "fs.h"

#include <QStringList>
#include <QVariant>

#include <KDEDModule>

class KDirWatch;

class StashNotifier : public KDEDModule
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kio.StashNotifier")

private:
    KDirWatch *dirWatch;
    StashFileSystem *fileSystem;
    const QString m_daemonService;
    const QString m_daemonPath;
    QString processString(const QString &path);
    QString encodeString(StashFileSystem::StashNode::iterator node, const QString &path);
    QString encodeString(StashFileSystem::StashNodeData nodeData, const QString &path);

public:
    StashNotifier(QObject *parent,
                  const QList<QVariant> &,
                  const QString &daemonService = "org.kde.kio.StashNotifier",
                  const QString &daemonPath = "/StashNotifier");
    ~StashNotifier();

Q_SIGNALS:
    Q_SCRIPTABLE void listChanged();

public Q_SLOTS:
    Q_SCRIPTABLE void addPath(const QString &source, const QString &stashPath, int fileType);
    Q_SCRIPTABLE void removePath(const QString &path);
    Q_SCRIPTABLE void nukeStash();
    Q_SCRIPTABLE void pingDaemon();
    Q_SCRIPTABLE bool copyWithStash(const QString &src, const QString &dest);
    Q_SCRIPTABLE QStringList fileList(const QString &path);
    Q_SCRIPTABLE QString fileInfo(const QString &path);

private Q_SLOTS:
    void dirty(const QString &path);
    void created(const QString &path);
    void removeWatchedPath(const QString &filePath);
    void displayRoot() // function to the contents of the SFS for testing
    {
        // fileSystem->displayRoot();
    }
};

#endif
