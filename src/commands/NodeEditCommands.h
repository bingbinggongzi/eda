#pragma once

#include <QUndoCommand>

#include <QPointF>
#include <QString>

class EditorScene;

class NodeMoveCommand : public QUndoCommand {
public:
    NodeMoveCommand(EditorScene* scene,
                    const QString& nodeId,
                    const QPointF& beforePos,
                    const QPointF& afterPos,
                    bool alreadyApplied,
                    QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;
    int id() const override;
    bool mergeWith(const QUndoCommand* other) override;

private:
    EditorScene* m_scene = nullptr;
    QString m_nodeId;
    QPointF m_beforePos;
    QPointF m_afterPos;
    bool m_alreadyApplied = true;
    bool m_firstRedo = true;
};

class NodeRenameCommand : public QUndoCommand {
public:
    NodeRenameCommand(EditorScene* scene,
                      const QString& nodeId,
                      const QString& beforeName,
                      const QString& afterName,
                      bool alreadyApplied,
                      QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;
    int id() const override;
    bool mergeWith(const QUndoCommand* other) override;

private:
    EditorScene* m_scene = nullptr;
    QString m_nodeId;
    QString m_beforeName;
    QString m_afterName;
    bool m_alreadyApplied = true;
    bool m_firstRedo = true;
};

class NodePropertyCommand : public QUndoCommand {
public:
    NodePropertyCommand(EditorScene* scene,
                        const QString& nodeId,
                        const QString& key,
                        const QString& beforeValue,
                        const QString& afterValue,
                        bool alreadyApplied,
                        QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;
    int id() const override;
    bool mergeWith(const QUndoCommand* other) override;

private:
    EditorScene* m_scene = nullptr;
    QString m_nodeId;
    QString m_key;
    QString m_beforeValue;
    QString m_afterValue;
    bool m_alreadyApplied = true;
    bool m_firstRedo = true;
};
