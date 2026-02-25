#pragma once

#include "../model/GraphDocument.h"

#include <QUndoCommand>

class EditorScene;

class DocumentStateCommand : public QUndoCommand {
public:
    DocumentStateCommand(EditorScene* scene,
                         const GraphDocument& before,
                         const GraphDocument& after,
                         const QString& text,
                         bool alreadyApplied,
                         QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    EditorScene* m_scene = nullptr;
    GraphDocument m_before;
    GraphDocument m_after;
    bool m_alreadyApplied = true;
    bool m_firstRedo = true;
};
