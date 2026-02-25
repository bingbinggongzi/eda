#include "MainWindow.h"

#include "GraphView.h"
#include "items/EdgeItem.h"
#include "items/NodeItem.h"
#include "scene/EditorScene.h"

#include <QDockWidget>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStyle>
#include <QTableWidget>
#include <QTabWidget>
#include <QToolBar>
#include <QToolBox>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setupWindow();
    setupMenusAndToolbar();
    setupCentralArea();
    setupLeftDocks();
    setupRightDock();
    setupSignalBindings();
    populateDemoGraph();
    rebuildProjectTreeNodes();
}

void MainWindow::setupWindow() {
    resize(1600, 940);
    setWindowTitle(QStringLiteral("EDA Editor Prototype"));
    setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks | QMainWindow::AnimatedDocks);
    statusBar()->showMessage(QStringLiteral("Ready"));
}

void MainWindow::setupMenusAndToolbar() {
    QMenu* projectMenu = menuBar()->addMenu(QStringLiteral("Project"));
    QMenu* editMenu = menuBar()->addMenu(QStringLiteral("Edit"));
    m_viewMenu = menuBar()->addMenu(QStringLiteral("View"));
    QMenu* runMenu = menuBar()->addMenu(QStringLiteral("Run"));
    menuBar()->addMenu(QStringLiteral("Help"));

    QAction* saveAction =
        projectMenu->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton), QStringLiteral("Save"));
    QAction* openAction =
        projectMenu->addAction(style()->standardIcon(QStyle::SP_DialogOpenButton), QStringLiteral("Open"));
    QAction* clearAction = projectMenu->addAction(QStringLiteral("Clear Graph"));
    projectMenu->addSeparator();
    projectMenu->addAction(QStringLiteral("Exit"), this, &QWidget::close);

    editMenu->addAction(style()->standardIcon(QStyle::SP_ArrowBack), QStringLiteral("Undo"));
    editMenu->addAction(style()->standardIcon(QStyle::SP_ArrowForward), QStringLiteral("Redo"));
    editMenu->addSeparator();
    editMenu->addAction(QStringLiteral("Delete"));

    runMenu->addAction(style()->standardIcon(QStyle::SP_MediaPlay), QStringLiteral("Run"));
    runMenu->addAction(style()->standardIcon(QStyle::SP_MediaStop), QStringLiteral("Stop"));

    QToolBar* toolBar = addToolBar(QStringLiteral("Main"));
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(20, 20));
    toolBar->addAction(openAction);
    toolBar->addAction(saveAction);
    toolBar->addSeparator();
    toolBar->addAction(clearAction);
    toolBar->addSeparator();
    toolBar->addAction(style()->standardIcon(QStyle::SP_MediaPlay), QStringLiteral("Run"));

    connect(clearAction, &QAction::triggered, this, [this]() {
        if (!m_scene) {
            return;
        }
        m_scene->clear();
        rebuildProjectTreeNodes();
        statusBar()->showMessage(QStringLiteral("Graph cleared"), 2000);
    });
}

void MainWindow::setupCentralArea() {
    m_editorTabs = new QTabWidget(this);
    m_editorTabs->setDocumentMode(true);
    m_editorTabs->setTabsClosable(true);

    m_scene = new EditorScene(this);
    m_scene->setSceneRect(0, 0, 3600, 2400);

    m_graphView = new GraphView(this);
    m_graphView->setScene(m_scene);

    QWidget* editorPage = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(editorPage);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QLabel* rulerHint = new QLabel(QStringLiteral("Coordinate ruler placeholder"), editorPage);
    rulerHint->setFixedHeight(20);
    rulerHint->setStyleSheet(
        "QLabel { color: #666; background: #f1f1f1; border-bottom: 1px solid #dadada; padding-left: 8px; }");
    layout->addWidget(rulerHint);
    layout->addWidget(m_graphView);

    m_editorTabs->addTab(editorPage, QStringLiteral("RCP_SH1805"));
    m_editorTabs->addTab(new QLabel(QStringLiteral("Reserved tab"), this), QStringLiteral("RCP_SH1208"));
    setCentralWidget(m_editorTabs);
}

void MainWindow::setupLeftDocks() {
    m_projectDock = new QDockWidget(QStringLiteral("Project"), this);
    m_projectDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_projectDock->setMinimumWidth(230);

    m_projectTree = new QTreeWidget(m_projectDock);
    m_projectTree->setHeaderHidden(true);
    QTreeWidgetItem* projectRoot = new QTreeWidgetItem(QStringList() << QStringLiteral("RCP_SH0005"));
    m_graphNodesRoot = new QTreeWidgetItem(QStringList() << QStringLiteral("Graph Nodes"));
    projectRoot->addChild(m_graphNodesRoot);
    m_projectTree->addTopLevelItem(projectRoot);
    projectRoot->setExpanded(true);
    m_graphNodesRoot->setExpanded(true);
    m_projectDock->setWidget(m_projectTree);
    addDockWidget(Qt::LeftDockWidgetArea, m_projectDock);

    m_propertyDock = new QDockWidget(QStringLiteral("Properties"), this);
    m_propertyDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_propertyDock->setMinimumHeight(220);

    m_propertyTable = new QTableWidget(8, 2, m_propertyDock);
    m_propertyTable->setHorizontalHeaderLabels({QStringLiteral("Key"), QStringLiteral("Value")});
    m_propertyTable->horizontalHeader()->setStretchLastSection(true);
    m_propertyTable->verticalHeader()->setVisible(false);
    m_propertyTable->setAlternatingRowColors(true);
    m_propertyTable->setShowGrid(false);
    m_propertyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_propertyDock->setWidget(m_propertyTable);
    addDockWidget(Qt::LeftDockWidgetArea, m_propertyDock);

    splitDockWidget(m_projectDock, m_propertyDock, Qt::Vertical);

    m_viewMenu->addAction(m_projectDock->toggleViewAction());
    m_viewMenu->addAction(m_propertyDock->toggleViewAction());
}

void MainWindow::setupRightDock() {
    m_paletteDock = new QDockWidget(QStringLiteral("Toolbox"), this);
    m_paletteDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_paletteDock->setMinimumWidth(280);

    QWidget* panel = new QWidget(m_paletteDock);
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    QToolBox* toolBox = new QToolBox(panel);
    addPaletteCategory(toolBox,
                       QStringLiteral("Actuator"),
                       {QStringLiteral("tm_CheckVlv"),
                        QStringLiteral("tm_AirWater"),
                        QStringLiteral("tm_Bound"),
                        QStringLiteral("tm_Node")});
    addPaletteCategory(toolBox,
                       QStringLiteral("Sensor"),
                       {QStringLiteral("tm_TubeHte"),
                        QStringLiteral("tm_HeatExChS"),
                        QStringLiteral("tm_HeatExCh"),
                        QStringLiteral("tm_Vessel")});
    addPaletteCategory(toolBox,
                       QStringLiteral("Control"),
                       {QStringLiteral("Voter"),
                        QStringLiteral("SFT"),
                        QStringLiteral("Sum"),
                        QStringLiteral("tm_Bearing")});
    addPaletteCategory(toolBox,
                       QStringLiteral("Electric"),
                       {QStringLiteral("tm_Load"),
                        QStringLiteral("tm_HeatSide"),
                        QStringLiteral("tm_Valve"),
                        QStringLiteral("tm_Pump")});

    layout->addWidget(toolBox);
    m_paletteDock->setWidget(panel);
    addDockWidget(Qt::RightDockWidgetArea, m_paletteDock);
    m_viewMenu->addAction(m_paletteDock->toggleViewAction());
}

void MainWindow::setupSignalBindings() {
    connect(m_graphView, &GraphView::paletteItemDropped, this, [this](const QString& typeName, const QPointF& scenePos) {
        if (!m_scene) {
            return;
        }
        NodeItem* node = m_scene->createNode(typeName, scenePos);
        if (node) {
            m_scene->clearSelection();
            node->setSelected(true);
            statusBar()->showMessage(QStringLiteral("Node created: %1").arg(typeName), 2000);
        }
    });

    connect(m_graphView, &GraphView::zoomChanged, this, [this](int percent) {
        statusBar()->showMessage(QStringLiteral("Zoom: %1%").arg(percent), 1500);
    });

    connect(m_scene, &EditorScene::selectionInfoChanged, this, &MainWindow::updatePropertyTable);
    connect(m_scene, &EditorScene::graphChanged, this, &MainWindow::rebuildProjectTreeNodes);

    connect(m_projectTree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item) {
        if (!item || !m_scene) {
            return;
        }
        const QString nodeId = item->data(0, Qt::UserRole).toString();
        if (nodeId.isEmpty()) {
            return;
        }

        for (QGraphicsItem* sceneItem : m_scene->items()) {
            if (NodeItem* node = dynamic_cast<NodeItem*>(sceneItem)) {
                if (node->nodeId() == nodeId) {
                    m_scene->clearSelection();
                    node->setSelected(true);
                    m_graphView->centerOn(node);
                    break;
                }
            }
        }
    });
}

void MainWindow::populateDemoGraph() {
    auto addNode = [this](const QString& type, const QString& display, const QPointF& pos) -> NodeItem* {
        NodeItem* node = m_scene->createNode(type, pos);
        if (node) {
            node->setDisplayName(display);
        }
        return node;
    };

    QGraphicsSimpleTextItem* text1 =
        m_scene->addSimpleText(QStringLiteral("Drop components from the right panel to create nodes."));
    text1->setPos(40.0, 40.0);
    text1->setBrush(QColor(85, 85, 85));

    NodeItem* n1 = addNode(QStringLiteral("Voter"), QStringLiteral("Voter"), QPointF(320, 180));
    NodeItem* n2 = addNode(QStringLiteral("SFT"), QStringLiteral("SFT"), QPointF(560, 200));
    NodeItem* n3 = addNode(QStringLiteral("Sum"), QStringLiteral("Sum"), QPointF(790, 240));
    NodeItem* n4 = addNode(QStringLiteral("tm_Node"), QStringLiteral("RCP0001KM"), QPointF(1020, 240));

    if (n1 && n2) {
        m_scene->createEdge(n1->firstOutputPort(), n2->firstInputPort());
    }
    if (n2 && n3) {
        m_scene->createEdge(n2->firstOutputPort(), n3->firstInputPort());
    }
    if (n3 && n4) {
        m_scene->createEdge(n3->firstOutputPort(), n4->firstInputPort());
    }
}

void MainWindow::addPaletteCategory(QToolBox* toolBox, const QString& title, const QStringList& names) {
    auto* list = new QListWidget(toolBox);
    list->setViewMode(QListView::IconMode);
    list->setResizeMode(QListView::Adjust);
    list->setIconSize(QSize(36, 36));
    list->setSpacing(8);
    list->setMovement(QListView::Static);
    list->setDragEnabled(true);
    list->setDragDropMode(QAbstractItemView::DragOnly);
    list->setDefaultDropAction(Qt::CopyAction);

    for (const QString& itemName : names) {
        auto* item = new QListWidgetItem(style()->standardIcon(QStyle::SP_FileDialogContentsView), itemName);
        item->setData(Qt::UserRole, itemName);
        list->addItem(item);
    }
    toolBox->addItem(list, title);
}

void MainWindow::updatePropertyTable(const QString& itemType,
                                     const QString& itemId,
                                     const QString& displayName,
                                     const QPointF& pos,
                                     int inputCount,
                                     int outputCount) {
    const QSignalBlocker blocker(m_propertyTable);
    auto setRow = [this](int row, const QString& key, const QString& val) {
        m_propertyTable->setItem(row, 0, new QTableWidgetItem(key));
        m_propertyTable->setItem(row, 1, new QTableWidgetItem(val));
    };

    setRow(0, QStringLiteral("Selection"), itemType.isEmpty() ? QStringLiteral("(none)") : itemType);
    setRow(1, QStringLiteral("ID"), itemId);
    setRow(2, QStringLiteral("Name"), displayName);
    setRow(3, QStringLiteral("X"), QString::number(pos.x(), 'f', 1));
    setRow(4, QStringLiteral("Y"), QString::number(pos.y(), 'f', 1));
    setRow(5, QStringLiteral("Inputs"), QString::number(inputCount));
    setRow(6, QStringLiteral("Outputs"), QString::number(outputCount));
    setRow(7, QStringLiteral("Grid Snap"), m_scene && m_scene->snapToGrid() ? QStringLiteral("On") : QStringLiteral("Off"));
}

void MainWindow::rebuildProjectTreeNodes() {
    if (!m_scene || !m_graphNodesRoot) {
        return;
    }

    m_graphNodesRoot->takeChildren();

    QVector<NodeItem*> nodes;
    for (QGraphicsItem* item : m_scene->items()) {
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
