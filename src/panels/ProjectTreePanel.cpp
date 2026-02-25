#include "ProjectTreePanel.h"

#include "items/NodeItem.h"
#include "scene/EditorScene.h"

#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

ProjectTreePanel::ProjectTreePanel(QWidget* parent)
    : QWidget(parent) {
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);

    QTreeWidgetItem* projectRoot = new QTreeWidgetItem(QStringList() << QStringLiteral("RCP_SH0005"));
    m_graphNodesRoot = new QTreeWidgetItem(QStringList() << QStringLiteral("Graph Nodes"));
    projectRoot->addChild(m_graphNodesRoot);
    m_tree->addTopLevelItem(projectRoot);
    projectRoot->setExpanded(true);
    m_graphNodesRoot->setExpanded(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item) {
        if (!item) {
            return;
        }
        const QString nodeId = item->data(0, Qt::UserRole).toString();
        if (!nodeId.isEmpty()) {
            emit nodeSelected(nodeId);
        }
    });
}

void ProjectTreePanel::rebuildFromScene(EditorScene* scene) {
    m_graphNodesRoot->takeChildren();
    if (!scene) {
        return;
    }

    QVector<NodeItem*> nodes;
    for (QGraphicsItem* item : scene->items()) {
        if (NodeItem* node = dynamic_cast<NodeItem*>(item)) {
            nodes.push_back(node);
        }
    }
    std::sort(nodes.begin(), nodes.end(), [](const NodeItem* a, const NodeItem* b) { return a->nodeId() < b->nodeId(); });

    for (NodeItem* node : nodes) {
        QTreeWidgetItem* child =
            new QTreeWidgetItem(QStringList() << QStringLiteral("%1 (%2)").arg(node->displayName(), node->nodeId()));
        child->setData(0, Qt::UserRole, node->nodeId());
        m_graphNodesRoot->addChild(child);
    }
    m_graphNodesRoot->setExpanded(true);
}

void ProjectTreePanel::selectNode(const QString& nodeId) {
    QTreeWidgetItem* item = findTreeItemByNodeId(nodeId);
    if (!item) {
        return;
    }
    const QSignalBlocker blocker(m_tree);
    m_tree->setCurrentItem(item);
}

QTreeWidgetItem* ProjectTreePanel::findTreeItemByNodeId(const QString& nodeId) const {
    if (!m_graphNodesRoot || nodeId.isEmpty()) {
        return nullptr;
    }
    for (int i = 0; i < m_graphNodesRoot->childCount(); ++i) {
        QTreeWidgetItem* child = m_graphNodesRoot->child(i);
        if (child && child->data(0, Qt::UserRole).toString() == nodeId) {
            return child;
        }
    }
    return nullptr;
}
