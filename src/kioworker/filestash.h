/*
 *   SPDX-FileCopyrightText: 2016 Arnav Dhamija <arnav.dhamija@gmail.com>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FILESTASH_H
#define FILESTASH_H

#include <KIO/WorkerBase>
#include <QObject>
#include <QString>

class FileStash : public QObject, public KIO::WorkerBase
{
    Q_OBJECT

public:
    FileStash(const QByteArray &pool, const QByteArray &app);
    ~FileStash() override;

    enum NodeType {
        DirectoryNode,
        SymlinkNode,
        FileNode,
        InvalidNode,
    };

    struct dirList {
        QString filePath;
        QString source;
        FileStash::NodeType type;
        dirList() {}
        ~dirList() {}
        dirList(const dirList &obj) {
            filePath = obj.filePath;
            source = obj.source;
            type = obj.type;
        }
    };

private:
    void createTopLevelDirEntry(KIO::UDSEntry &entry);
    bool isRoot(const QString &string);
    bool createUDSEntry(KIO::UDSEntry &entry, const FileStash::dirList &fileItem);
    bool copyFileToStash(const QUrl &src, const QUrl &dest);
    bool copyStashToFile(const QUrl &src, const QUrl &dest, KIO::JobFlags flags);
    bool moveStashToFile(const QUrl &src, const QUrl &dest, KIO::JobFlags flags);

    QStringList setFileList(const QUrl &url);
    QString setFileInfo(const QUrl &url);
    FileStash::dirList createDirListItem(const QString &fileInfo);

    const QString m_daemonService = QStringLiteral("org.kde.kio.StashNotifier");
    const QString m_daemonPath = QStringLiteral("/StashNotifier");

public:
    KIO::WorkerResult get(const QUrl &url) override;
    KIO::WorkerResult listDir(const QUrl &url) override;
    KIO::WorkerResult copy(const QUrl &src, const QUrl &dest, int permissions, KIO::JobFlags flags) override;
    KIO::WorkerResult mkdir(const QUrl &url, int permissions) override;
    KIO::WorkerResult del(const QUrl &url, bool isFile) override;
    KIO::WorkerResult stat(const QUrl &url) override;
    KIO::WorkerResult rename(const QUrl &src, const QUrl &dest, KIO::JobFlags flags) override;
};

#endif
