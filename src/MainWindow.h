#pragma once

#include <QMainWindow>

class GraphView;
class QDockWidget;
class QGraphicsRectItem;
class QGraphicsScene;
class QMenu;
class QTableWidget;
class QTabWidget;
class QTreeWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void setupWindow();
    void setupMenusAndToolbar();
    void setupCentralArea();
    void setupLeftDocks();
    void setupRightDock();
    void populateDemoGraph();

    QMenu* m_viewMenu = nullptr;
    QTabWidget* m_editorTabs = nullptr;
    GraphView* m_graphView = nullptr;
    QGraphicsScene* m_scene = nullptr;
    QTreeWidget* m_projectTree = nullptr;
    QTableWidget* m_propertyTable = nullptr;

    QDockWidget* m_projectDock = nullptr;
    QDockWidget* m_propertyDock = nullptr;
    QDockWidget* m_paletteDock = nullptr;
};
