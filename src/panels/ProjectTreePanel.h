#pragma once

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class EditorScene;

class ProjectTreePanel : public QWidget {
    Q_OBJECT

public:
    explicit ProjectTreePanel(QWidget* parent = nullptr);

    void rebuildFromScene(EditorScene* scene);
    void selectNode(const QString& nodeId);

signals:
    void nodeSelected(const QString& nodeId);

private:
    QTreeWidgetItem* findTreeItemByNodeId(const QString& nodeId) const;

    QTreeWidget* m_tree = nullptr;
    QTreeWidgetItem* m_graphNodesRoot = nullptr;
};
