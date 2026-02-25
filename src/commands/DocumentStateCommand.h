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
                         const QString& mergeKey = QString(),
                         QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;
    int id() const override;
    bool mergeWith(const QUndoCommand* other) override;

private:
    EditorScene* m_scene = nullptr;
    GraphDocument m_before;
    GraphDocument m_after;
    bool m_alreadyApplied = true;
    bool m_firstRedo = true;
    QString m_mergeKey;
};
