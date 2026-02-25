#include "NodeEditCommands.h"

#include "scene/EditorScene.h"

namespace {
int mergeId(const QString& key) {
    return static_cast<int>(qHash(key) & 0x7FFFFFFF);
}
}  // namespace

NodeMoveCommand::NodeMoveCommand(EditorScene* scene,
                                 const QString& nodeId,
                                 const QPointF& beforePos,
                                 const QPointF& afterPos,
                                 bool alreadyApplied,
                                 QUndoCommand* parent)
    : QUndoCommand(QStringLiteral("Move Node"), parent),
      m_scene(scene),
      m_nodeId(nodeId),
      m_beforePos(beforePos),
      m_afterPos(afterPos),
      m_alreadyApplied(alreadyApplied) {}

void NodeMoveCommand::undo() {
    if (m_scene) {
        m_scene->applyNodePositionInternal(m_nodeId, m_beforePos, true);
    }
}

void NodeMoveCommand::redo() {
    if (m_firstRedo && m_alreadyApplied) {
        m_firstRedo = false;
        return;
    }
    m_firstRedo = false;
    if (m_scene) {
        m_scene->applyNodePositionInternal(m_nodeId, m_afterPos, true);
    }
}

int NodeMoveCommand::id() const {
    return mergeId(QStringLiteral("move:%1").arg(m_nodeId));
}

bool NodeMoveCommand::mergeWith(const QUndoCommand* other) {
    const auto* rhs = dynamic_cast<const NodeMoveCommand*>(other);
    if (!rhs || rhs->m_nodeId != m_nodeId) {
        return false;
    }
    m_afterPos = rhs->m_afterPos;
    return true;
}

NodeRenameCommand::NodeRenameCommand(EditorScene* scene,
                                     const QString& nodeId,
                                     const QString& beforeName,
                                     const QString& afterName,
                                     bool alreadyApplied,
                                     QUndoCommand* parent)
    : QUndoCommand(QStringLiteral("Rename Node"), parent),
      m_scene(scene),
      m_nodeId(nodeId),
      m_beforeName(beforeName),
      m_afterName(afterName),
      m_alreadyApplied(alreadyApplied) {}

void NodeRenameCommand::undo() {
    if (m_scene) {
        m_scene->applyNodeRenameInternal(m_nodeId, m_beforeName, true);
    }
}

void NodeRenameCommand::redo() {
    if (m_firstRedo && m_alreadyApplied) {
        m_firstRedo = false;
        return;
    }
    m_firstRedo = false;
    if (m_scene) {
        m_scene->applyNodeRenameInternal(m_nodeId, m_afterName, true);
    }
}

int NodeRenameCommand::id() const {
    return mergeId(QStringLiteral("rename:%1").arg(m_nodeId));
}

bool NodeRenameCommand::mergeWith(const QUndoCommand* other) {
    const auto* rhs = dynamic_cast<const NodeRenameCommand*>(other);
    if (!rhs || rhs->m_nodeId != m_nodeId) {
        return false;
    }
    m_afterName = rhs->m_afterName;
    return true;
}

NodePropertyCommand::NodePropertyCommand(EditorScene* scene,
                                         const QString& nodeId,
                                         const QString& key,
                                         const QString& beforeValue,
                                         const QString& afterValue,
                                         bool alreadyApplied,
                                         QUndoCommand* parent)
    : QUndoCommand(QStringLiteral("Edit Property"), parent),
      m_scene(scene),
      m_nodeId(nodeId),
      m_key(key),
      m_beforeValue(beforeValue),
      m_afterValue(afterValue),
      m_alreadyApplied(alreadyApplied) {}

void NodePropertyCommand::undo() {
    if (m_scene) {
        m_scene->applyNodePropertyInternal(m_nodeId, m_key, m_beforeValue, true);
    }
}

void NodePropertyCommand::redo() {
    if (m_firstRedo && m_alreadyApplied) {
        m_firstRedo = false;
        return;
    }
    m_firstRedo = false;
    if (m_scene) {
        m_scene->applyNodePropertyInternal(m_nodeId, m_key, m_afterValue, true);
    }
}

int NodePropertyCommand::id() const {
    return mergeId(QStringLiteral("prop:%1:%2").arg(m_nodeId, m_key));
}

bool NodePropertyCommand::mergeWith(const QUndoCommand* other) {
    const auto* rhs = dynamic_cast<const NodePropertyCommand*>(other);
    if (!rhs || rhs->m_nodeId != m_nodeId || rhs->m_key != m_key) {
        return false;
    }
    m_afterValue = rhs->m_afterValue;
    return true;
}
