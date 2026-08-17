/*
 *   SPDX-FileCopyrightText: 2016 Arnav Dhamija <arnav.dhamija@gmail.com>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "filestash.h"
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QUrl>
#include <KLocalizedString>
#include <KIO/CopyJob>
#include <KIO/Job>

class KIOPluginForMetaData : public QObject {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.worker.filestash" FILE "filestash.json")
};

extern "C" {
    int Q_DECL_EXPORT kdemain(int argc, char **argv) {
        QCoreApplication app(argc, argv);
        FileStash worker(argv[2], argv[3]);
        worker.dispatchLoop();
        return 0;
    }
}

FileStash::FileStash(const QByteArray &pool, const QByteArray &app)
    : QObject(nullptr)
    , KIO::WorkerBase("stash", pool, app) {}

FileStash::~FileStash() {}

void FileStash::createTopLevelDirEntry(KIO::UDSEntry &entry) {
    entry.clear();
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, 0040000);
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, 0777);
    entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));
}

QStringList FileStash::setFileList(const QUrl &url) {
    QDBusMessage msg = QDBusMessage::createMethodCall(m_daemonService, m_daemonPath, "", "fileList");
    msg << url.path();
    QDBusReply<QStringList> received = QDBusConnection::sessionBus().call(msg);
    return received.value();
}

QString FileStash::setFileInfo(const QUrl &url) {
    QDBusMessage msg = QDBusMessage::createMethodCall(m_daemonService, m_daemonPath, "", "fileInfo");
    msg << url.path();
    QDBusReply<QString> received = QDBusConnection::sessionBus().call(msg);
    return received.value();
}

KIO::WorkerResult FileStash::get(const QUrl &url) {
    QString fileInfo = setFileInfo(url);
    FileStash::dirList item = createDirListItem(fileInfo);
    if (item.type == NodeType::InvalidNode || item.source.isEmpty()) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }
    if (item.type == NodeType::DirectoryNode) {
        return KIO::WorkerResult::fail(KIO::ERR_IS_DIRECTORY, url.toDisplayString());
    }

    QFile file(item.source);
    if (!file.open(QIODevice::ReadOnly)) {
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_READ, item.source);
    }

    QMimeDatabase mimeDatabase;
    QMimeType mime = mimeDatabase.mimeTypeForFile(item.source);
    mimeType(mime.name());
    totalSize(file.size());

    QByteArray buffer;
    buffer.resize(1024 * 64);
    while (!file.atEnd()) {
        qint64 bytesRead = file.read(buffer.data(), buffer.size());
        if (bytesRead > 0) {
            data(buffer.first(bytesRead));
        }
    }
    file.close();
    data(QByteArray());
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult FileStash::stat(const QUrl &url) {
    KIO::UDSEntry entry;
    if (isRoot(url.path())) {
        createTopLevelDirEntry(entry);
    } else {
        QString fileInfo = setFileInfo(url);
        FileStash::dirList item = createDirListItem(fileInfo);
        if (!createUDSEntry(entry, item)) {
            return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
        }
    }
    statEntry(entry);
    return KIO::WorkerResult::pass();
}

bool FileStash::createUDSEntry(KIO::UDSEntry &entry, const FileStash::dirList &fileItem) {
    QMimeDatabase mimeDatabase;
    QString stringFilePath = fileItem.filePath;
    QString name = QFileInfo(stringFilePath).fileName();
    if (name.isEmpty()) {
        name = stringFilePath.split(QLatin1Char('/')).last();
    }

    switch (fileItem.type) {
    case NodeType::DirectoryNode:
        entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, 0040000);
        entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, name);
        entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, name);
        entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, 0777);
        break;
    case NodeType::InvalidNode:
        return false;
    default:
        QFileInfo entryInfo(fileItem.source);
        QMimeType fileMimetype = mimeDatabase.mimeTypeForFile(fileItem.source);
        entry.fastInsert(KIO::UDSEntry::UDS_TARGET_URL, QUrl::fromLocalFile(fileItem.source).toString());
        entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, fileMimetype.name());
        entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, QStringLiteral("%1  [%2]").arg(name, fileItem.source));
        entry.fastInsert(KIO::UDSEntry::UDS_COMMENT, fileItem.source);
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, name);
        entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, 0666);
        entry.fastInsert(KIO::UDSEntry::UDS_SIZE, entryInfo.size());
        entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, entryInfo.lastModified().toSecsSinceEpoch());
        entry.fastInsert(KIO::UDSEntry::UDS_ACCESS_TIME, entryInfo.lastRead().toSecsSinceEpoch());

        if (fileItem.type == NodeType::FileNode) {
            entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, 0100000);
        } else if (fileItem.type == NodeType::SymlinkNode) {
            entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, 0120000);
            entry.fastInsert(KIO::UDSEntry::UDS_LINK_DEST, fileItem.source);
        } else {
            return false;
        }
    }
    return true;
}

FileStash::dirList FileStash::createDirListItem(const QString &fileInfo) {
    QStringList strings = fileInfo.split("::", Qt::KeepEmptyParts);
    FileStash::dirList item;
    if (strings.isEmpty() || strings.size() < 3) {
        item.type = FileStash::NodeType::InvalidNode;
        return item;
    }
    if (strings.at(0) == "dir") item.type = FileStash::NodeType::DirectoryNode;
    else if (strings.at(0) == "file") item.type = FileStash::NodeType::FileNode;
    else if (strings.at(0) == "symlink") item.type = FileStash::NodeType::SymlinkNode;
    else item.type = FileStash::NodeType::InvalidNode;
    item.filePath = strings.at(1);
    item.source = strings.at(2);
    return item;
}

KIO::WorkerResult FileStash::listDir(const QUrl &url) {
    QStringList fileList = setFileList(url);
    KIO::UDSEntry entry;
    if (isRoot(url.path())) {
        createTopLevelDirEntry(entry);
        listEntry(entry);
    } else {
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
        entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, 0040000);
        entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));
        entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, 0777);
        listEntry(entry);
    }
    if (fileList.isEmpty() || fileList.at(0) == "error::error::InvalidNode") {
        return KIO::WorkerResult::pass();
    }
    for (const QString &file : fileList) {
        entry.clear();
        FileStash::dirList item = createDirListItem(file);
        if (createUDSEntry(entry, item)) {
            listEntry(entry);
        }
    }
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult FileStash::mkdir(const QUrl &url, int permissions) {
    Q_UNUSED(permissions)
    QDBusMessage msg = QDBusMessage::createMethodCall(m_daemonService, m_daemonPath, "", "addPath");
    msg << "" << url.path() << (int)NodeType::DirectoryNode;
    QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("Could not create a directory"));
    }
    return KIO::WorkerResult::pass();
}

bool FileStash::copyFileToStash(const QUrl &src, const QUrl &dest) {
    NodeType fileType;
    QString srcPath = src.isLocalFile() ? src.toLocalFile() : src.path();
    QFileInfo fileInfo(srcPath);
    if (fileInfo.isSymLink()) fileType = NodeType::SymlinkNode;
    else if (fileInfo.isDir()) fileType = NodeType::DirectoryNode;
    else fileType = NodeType::FileNode;

    QDBusMessage msg = QDBusMessage::createMethodCall(m_daemonService, m_daemonPath, "", "addPath");
    msg << srcPath << dest.path() << (int)fileType;
    QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    return reply.type() != QDBusMessage::ErrorMessage;
}

bool FileStash::copyStashToFile(const QUrl &src, const QUrl &dest, KIO::JobFlags flags) {
    Q_UNUSED(flags)
    QString fileInfo = setFileInfo(src);
    FileStash::dirList item = createDirListItem(fileInfo);
    if (item.type == NodeType::InvalidNode || item.source.isEmpty()) {
        return false;
    }

    QString destPath = dest.isLocalFile() ? dest.toLocalFile() : dest.path();
    QFileInfo destInfo(destPath);
    QDir().mkpath(destInfo.absolutePath());
    if (QFile::exists(destPath)) {
        QFile::remove(destPath);
    }
    if (QFile::copy(item.source, destPath)) {
        return true;
    }
    QFile srcFile(item.source);
    QFile dstFile(destPath);
    if (srcFile.open(QIODevice::ReadOnly) && dstFile.open(QIODevice::WriteOnly)) {
        while (!srcFile.atEnd()) {
            dstFile.write(srcFile.read(65536));
        }
        dstFile.close();
        srcFile.close();
        return true;
    }
    return false;
}

bool FileStash::moveStashToFile(const QUrl &src, const QUrl &dest, KIO::JobFlags flags) {
    Q_UNUSED(flags)
    QString fileInfo = setFileInfo(src);
    FileStash::dirList item = createDirListItem(fileInfo);
    if (item.type != NodeType::InvalidNode && !item.source.isEmpty()) {
        QString destPath = dest.isLocalFile() ? dest.toLocalFile() : dest.path();
        QFileInfo destInfo(destPath);
        QDir().mkpath(destInfo.absolutePath());
        if (QFile::exists(destPath)) {
            QFile::remove(destPath);
        }
        if (QFile::rename(item.source, destPath)) {
            del(src, true);
            return true;
        }
        if (QFile::copy(item.source, destPath)) {
            QFile::remove(item.source);
            del(src, true);
            return true;
        }
        QFile srcFile(item.source);
        QFile dstFile(destPath);
        if (srcFile.open(QIODevice::ReadOnly) && dstFile.open(QIODevice::WriteOnly)) {
            while (!srcFile.atEnd()) {
                dstFile.write(srcFile.read(65536));
            }
            dstFile.close();
            srcFile.close();
            QFile::remove(item.source);
            del(src, true);
            return true;
        }
    }
    return false;
}

KIO::WorkerResult FileStash::copy(const QUrl &src, const QUrl &dest, int permissions, KIO::JobFlags flags) {
    Q_UNUSED(permissions)
    QString fileName = src.fileName();
    if (fileName.isEmpty()) {
        fileName = src.path().split(QLatin1Char('/')).last();
    }
    QUrl newDestPath = QUrl(dest.adjusted(QUrl::RemoveFilename).toString() + fileName);

    if (src.scheme() != "stash" && dest.scheme() == "stash") {
        if (copyFileToStash(src, newDestPath)) {
            return KIO::WorkerResult::pass();
        }
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("Could not copy to stash."));
    }

    if (src.scheme() == "stash" && dest.scheme() != "stash") {
        if (copyStashToFile(src, newDestPath, flags)) {
            return KIO::WorkerResult::pass();
        }
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("Could not copy from stash."));
    }

    if (src.scheme() == "stash" && dest.scheme() == "stash") {
        QString fileInfo = setFileInfo(src);
        FileStash::dirList item = createDirListItem(fileInfo);
        if (item.type != NodeType::InvalidNode && !item.source.isEmpty()) {
            if (copyFileToStash(QUrl::fromLocalFile(item.source), newDestPath)) {
                return KIO::WorkerResult::pass();
            }
        }
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("Could not copy in stash."));
    }

    return KIO::WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION, src.scheme());
}

KIO::WorkerResult FileStash::del(const QUrl &url, bool isFile) {
    Q_UNUSED(isFile)
    QDBusMessage msg = QDBusMessage::createMethodCall(m_daemonService, m_daemonPath, "", "removePath");
    msg << url.path();
    QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() != QDBusMessage::ErrorMessage) return KIO::WorkerResult::pass();
    return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, QString("Could not reach the stash daemon"));
}

KIO::WorkerResult FileStash::rename(const QUrl &src, const QUrl &dest, KIO::JobFlags flags) {
    if (src.scheme() == "file" && dest.scheme() == "stash") {
        if (copyFileToStash(src, dest)) return KIO::WorkerResult::pass();
    }
    if (src.scheme() == "stash" && dest.scheme() == "stash") {
        QString fileInfo = setFileInfo(src);
        FileStash::dirList item = createDirListItem(fileInfo);
        if (item.type != NodeType::InvalidNode && !item.source.isEmpty()) {
            if (copyFileToStash(QUrl::fromLocalFile(item.source), dest)) {
                del(src, true);
                return KIO::WorkerResult::pass();
            }
        }
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("Could not rename in stash."));
    }
    if (src.scheme() == "stash" && dest.scheme() != "stash") {
        if (moveStashToFile(src, dest, flags)) {
            return KIO::WorkerResult::pass();
        }
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("Could not move from stash."));
    }
    return KIO::WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION);
}

bool FileStash::isRoot(const QString &string) {
    return string.isEmpty() || string == "/";
}

#include "filestash.moc"
