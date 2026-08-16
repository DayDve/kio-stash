/*
 * SPDX-FileCopyrightText: 2016 Boudhayan Gupta <bgupta@kde.org>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef STASHFS_H
#define STASHFS_H

#include <QDebug>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>

class StashFileSystem : public QObject
{
    Q_OBJECT

public:
    enum NodeType {
        DirectoryNode,
        SymlinkNode,
        FileNode,
        InvalidNode,
    };

    struct StashNodeData;

    typedef QHash<QString, StashNodeData> StashNode;

    struct StashNodeData {
        StashNodeData(NodeType ntype)
            : type(ntype)
            , children(nullptr)
        {
        }
        ~StashNodeData()
        {
        }

        NodeType type;
        QString source;
        StashFileSystem::StashNode *children;
    };

    explicit StashFileSystem(QObject *parent = nullptr);
    virtual ~StashFileSystem();

    QStringList findNodesFromPath(const QString &path);

    bool delEntry(const QString &path);
    bool addFile(const QString &src, const QString &dest);
    bool addFolder(const QString &dest);
    bool addSymlink(const QString &src, const QString &dest);
    bool copyFile(const QString &src, const QString &dest);
    void deleteAllItems();

    // Finds the node object for the given path in the SFS
    StashNodeData findNode(const QString &path);
    StashNodeData findNode(const QStringList &path);

    StashNodeData getRoot();
    void findPathFromSource(const QString &path, const QString &dir, QStringList &fileList, StashNode *node);

    // For debug purposes
    void displayNode(StashNode *node);
    void displayRoot();

private:
    bool addNode(const QString &location, const StashNodeData &data);
    void deleteChildren(StashNodeData nodeData);
    QStringList splitPath(const QString &path);
    StashNodeData root;
};

#endif
