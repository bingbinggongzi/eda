#pragma once

#include <QMainWindow>
#include <QPointF>
#include <QString>

class EditorScene;
class GraphView;
class QDockWidget;
class QToolBox;
class QMenu;
class QTableWidget;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

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
    void setupSignalBindings();
    void populateDemoGraph();
    void addPaletteCategory(QToolBox* toolBox, const QString& title, const QStringList& names);
    void updatePropertyTable(const QString& itemType,
                             const QString& itemId,
                             const QString& displayName,
                             const QPointF& pos,
                             int inputCount,
                             int outputCount);
    void rebuildProjectTreeNodes();

    QMenu* m_viewMenu = nullptr;
    QTabWidget* m_editorTabs = nullptr;
    GraphView* m_graphView = nullptr;
    EditorScene* m_scene = nullptr;
    QTreeWidget* m_projectTree = nullptr;
    QTableWidget* m_propertyTable = nullptr;

    QDockWidget* m_projectDock = nullptr;
    QDockWidget* m_propertyDock = nullptr;
    QDockWidget* m_paletteDock = nullptr;

    QTreeWidgetItem* m_graphNodesRoot = nullptr;
    QString m_currentFilePath;
};
