#pragma once

#include <QWidget>

class EditorScene;
class QTableWidget;
class QTableWidgetItem;
class QPushButton;

class LayerPanel : public QWidget {
    Q_OBJECT

public:
    explicit LayerPanel(QWidget* parent = nullptr);

    void setScene(EditorScene* scene);

private:
    void refresh();
    QString layerIdAtRow(int row) const;
    int currentLayerRow() const;

    EditorScene* m_scene = nullptr;
    QTableWidget* m_table = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QPushButton* m_upButton = nullptr;
    QPushButton* m_downButton = nullptr;
    QPushButton* m_activeButton = nullptr;
    QPushButton* m_moveSelectionButton = nullptr;
    bool m_updating = false;
};
