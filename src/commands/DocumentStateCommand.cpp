#include "DocumentStateCommand.h"

#include "../scene/EditorScene.h"

DocumentStateCommand::DocumentStateCommand(EditorScene* scene,
                                           const GraphDocument& before,
                                           const GraphDocument& after,
                                           const QString& text,
                                           bool alreadyApplied,
                                           const QString& mergeKey,
                                           QUndoCommand* parent)
    : QUndoCommand(text, parent),
      m_scene(scene),
      m_before(before),
      m_after(after),
      m_alreadyApplied(alreadyApplied),
      m_mergeKey(mergeKey) {}

void DocumentStateCommand::undo() {
    if (m_scene) {
        m_scene->fromDocument(m_before);
    }
}

void DocumentStateCommand::redo() {
    if (m_firstRedo && m_alreadyApplied) {
        m_firstRedo = false;
        return;
    }
    m_firstRedo = false;
    if (m_scene) {
        m_scene->fromDocument(m_after);
    }
}

int DocumentStateCommand::id() const {
    if (m_mergeKey.isEmpty()) {
        return -1;
    }
    return static_cast<int>(qHash(m_mergeKey) & 0x7FFFFFFF);
}

bool DocumentStateCommand::mergeWith(const QUndoCommand* other) {
    if (!other || other->id() != id() || id() == -1) {
        return false;
    }
    const auto* rhs = dynamic_cast<const DocumentStateCommand*>(other);
    if (!rhs || rhs->m_mergeKey != m_mergeKey) {
        return false;
    }
    m_after = rhs->m_after;
    return true;
}
