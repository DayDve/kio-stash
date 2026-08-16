/*
 * SPDX-FileCopyrightText: 2016 Boudhayan Gupta <bgupta@kde.org>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fs.h"

StashFileSystem::StashFileSystem(QObject *parent)
    : QObject(parent)
    , root(DirectoryNode)
{
    root.children = new StashNode();
    displayRoot();
}

StashFileSystem::~StashFileSystem()
{
    deleteChildren(root);
}

StashFileSystem::StashNodeData StashFileSystem::getRoot()
{
    return root;
}

void StashFileSystem::findPathFromSource(const QString &path, const QString &dir, QStringList &fileList, StashNode *node)
{
    for (auto it = node->begin(); it != node->end(); ++it) {
        if (it.value().source == path) {
            fileList.append(dir + QLatin1Char('/') + it.key());
        }
        if (it.value().type == DirectoryNode) {
            findPathFromSource(path, dir + QLatin1Char('/') + it.key(), fileList, it.value().children);
        }
    }
}

void StashFileSystem::deleteChildren(StashNodeData nodeData)
{
    if (nodeData.children != nullptr) {
        Q_FOREACH (auto data, nodeData.children->values()) {
            deleteChildren(data);
        }
        delete nodeData.children;
        nodeData.children = nullptr;
    }
}

QStringList StashFileSystem::splitPath(const QString &path)
{
    QString filePath = path;
    if (filePath.startsWith(QLatin1Char('/'))) {
        filePath = filePath.right(filePath.size() - 1);
    }

    if (filePath.endsWith(QLatin1Char('/'))) {
        filePath = filePath.left(filePath.size() - 1);
    }
    return filePath.split(QLatin1Char('/'));
}

bool StashFileSystem::delEntry(const QString &location)
{
    QStringList path = splitPath(location);
    QString name = path.takeLast();
    StashNodeData baseData = findNode(path);
    if (!(baseData.type == DirectoryNode)) {
        return false;
    }

    if (!(baseData.children->contains(name))) {
        return false;
    }

    deleteChildren(baseData.children->value(name, StashNodeData(InvalidNode)));
    return (baseData.children->remove(name) > 0);
}

bool StashFileSystem::addNode(const QString &location, const StashNodeData &data)
{
    QStringList path = splitPath(location);
    QString name = path.takeLast();
    StashNodeData baseData = findNode(path);

    if (!(baseData.type == DirectoryNode)) {
        deleteChildren(data);
        return false;
    }

    baseData.children->insert(name, data);
    return true;
}

bool StashFileSystem::addFile(const QString &src, const QString &dest)
{
    StashNodeData fileData(FileNode);
    fileData.source = src;
    return addNode(dest, fileData);
}

bool StashFileSystem::addSymlink(const QString &src, const QString &dest)
{
    StashNodeData fileData(SymlinkNode);
    fileData.source = src;
    return addNode(dest, fileData);
}

bool StashFileSystem::addFolder(const QString &dest)
{
    StashNodeData fileData(DirectoryNode);
    fileData.source = QStringLiteral("");
    fileData.children = new StashNode();

    return addNode(dest, fileData);
}

bool StashFileSystem::copyFile(const QString &src, const QString &dest)
{
    StashNodeData fileToCopy = findNode(src);
    return addNode(dest, fileToCopy);
}

StashFileSystem::StashNodeData StashFileSystem::findNode(const QString &path)
{
    return findNode(splitPath(path));
}

StashFileSystem::StashNodeData StashFileSystem::findNode(const QStringList &path)
{
    StashNode *node = root.children;
    StashNodeData data = StashNodeData(InvalidNode);
    if (!path.size() || path.at(0).isEmpty()) {
        return root;
    } else {
        for (int i = 0; i < path.size(); ++i) {
            if (node->contains(path[i])) {
                data = node->value(path[i], StashNodeData(InvalidNode));
                if (data.type == DirectoryNode) {
                    node = data.children;
                }
                if (i == path.size() - 1) {
                    return data;
                }
            } else {
                return StashNodeData(InvalidNode);
            }
        }
        return StashNodeData(InvalidNode);
    }
}

void StashFileSystem::deleteAllItems()
{
    deleteChildren(root);
}

void StashFileSystem::displayNode(StashNode *node)
{
    for (auto it = node->begin(); it != node->end(); ++it) {
        qDebug() << "Stash Path" << it.key();
        qDebug() << "File Path" << it.value().source;
        qDebug() << "File Type" << it.value().type;
        if (it.value().type == DirectoryNode) {
            qDebug() << "Parent" << it.key();
            displayNode(it.value().children);
        }
    }
    return;
}

void StashFileSystem::displayRoot()
{
    displayNode(root.children);
}
