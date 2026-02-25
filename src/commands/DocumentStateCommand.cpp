#include "DocumentStateCommand.h"

#include "../scene/EditorScene.h"

DocumentStateCommand::DocumentStateCommand(EditorScene* scene,
                                           const GraphDocument& before,
                                           const GraphDocument& after,
                                           const QString& text,
                                           bool alreadyApplied,
                                           QUndoCommand* parent)
    : QUndoCommand(text, parent),
      m_scene(scene),
      m_before(before),
      m_after(after),
      m_alreadyApplied(alreadyApplied) {}

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
