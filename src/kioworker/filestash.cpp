#include "filestash.h"
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QUrl>
#include <KLocalizedString>

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
    QString name = QUrl(stringFilePath).fileName();

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
        entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, name);
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, name);
        entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, 0666);
        entry.fastInsert(KIO::UDSEntry::UDS_SIZE, entryInfo.size());

        if (fileItem.type == NodeType::FileNode) {
            entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, 0100000);
        } else if (fileItem.type == NodeType::SymlinkNode) {
            entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, 0120000);
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
    QFileInfo fileInfo(src.path());
    if (fileInfo.isSymLink()) fileType = NodeType::SymlinkNode;
    else if (fileInfo.isDir()) fileType = NodeType::DirectoryNode;
    else fileType = NodeType::FileNode;

    QDBusMessage msg = QDBusMessage::createMethodCall(m_daemonService, m_daemonPath, "", "addPath");
    msg << src.path() << dest.path() << (int)fileType;
    QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    return reply.type() != QDBusMessage::ErrorMessage;
}

KIO::WorkerResult FileStash::copy(const QUrl &src, const QUrl &dest, int permissions, KIO::JobFlags flags) {
    Q_UNUSED(permissions)
    Q_UNUSED(flags)
    QString fileName = src.fileName();
    if (fileName.isEmpty()) {
        fileName = src.path().split("/").last();
    }
    QUrl newDestPath = QUrl(dest.adjusted(QUrl::RemoveFilename).toString() + fileName);

    if (src.scheme() != "stash" && dest.scheme() == "stash") {
        if (copyFileToStash(src, newDestPath)) {
            return KIO::WorkerResult::pass();
        }
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("Could not copy."));
    }
    return KIO::WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION, src.scheme());
}

KIO::WorkerResult FileStash::del(const QUrl &url, bool isFile) {
    Q_UNUSED(isFile)
    QDBusMessage msg = QDBusMessage::createMethodCall(m_daemonService, m_daemonPath, "", "removePath");
    if (isRoot(url.adjusted(QUrl::RemoveFilename).toString())) msg << url.fileName();
    else msg << url.path();
    QDBusMessage reply = QDBusConnection::sessionBus().call(msg);
    if (reply.type() != QDBusMessage::ErrorMessage) return KIO::WorkerResult::pass();
    return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, QString("Could not reach the stash daemon"));
}

KIO::WorkerResult FileStash::rename(const QUrl &src, const QUrl &dest, KIO::JobFlags flags) {
    Q_UNUSED(flags)
    if (src.scheme() == "file" && dest.scheme() == "stash") {
        if (copyFileToStash(src, dest)) return KIO::WorkerResult::pass();
    }
    return KIO::WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION);
}

bool FileStash::isRoot(const QString &string) {
    return string.isEmpty() || string == "/";
}

#include "filestash.moc"
