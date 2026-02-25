#include "MainWindow.h"

#include "GraphView.h"
#include "items/EdgeItem.h"
#include "items/NodeItem.h"
#include "model/ComponentCatalog.h"
#include "model/GraphSerializer.h"
#include "scene/EditorScene.h"

#include <QDockWidget>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyle>
#include <QTableWidget>
#include <QTabWidget>
#include <QToolBar>
#include <QToolBox>
#include <QTreeWidget>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QMessageBox>

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
    m_undoStack = new QUndoStack(this);

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

    QAction* undoAction = m_undoStack->createUndoAction(this, QStringLiteral("Undo"));
    undoAction->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    QAction* redoAction = m_undoStack->createRedoAction(this, QStringLiteral("Redo"));
    redoAction->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    editMenu->addAction(undoAction);
    editMenu->addAction(redoAction);
    editMenu->addSeparator();
    QAction* deleteAction = editMenu->addAction(QStringLiteral("Delete"));
    deleteAction->setShortcut(QKeySequence::Delete);

    runMenu->addAction(style()->standardIcon(QStyle::SP_MediaPlay), QStringLiteral("Run"));
    runMenu->addAction(style()->standardIcon(QStyle::SP_MediaStop), QStringLiteral("Stop"));

    QToolBar* toolBar = addToolBar(QStringLiteral("Main"));
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(20, 20));
    toolBar->addAction(openAction);
    toolBar->addAction(saveAction);
    toolBar->addSeparator();
    toolBar->addAction(undoAction);
    toolBar->addAction(redoAction);
    toolBar->addSeparator();
    toolBar->addAction(clearAction);
    toolBar->addSeparator();
    toolBar->addAction(style()->standardIcon(QStyle::SP_MediaPlay), QStringLiteral("Run"));

    connect(clearAction, &QAction::triggered, this, [this]() {
        if (!m_scene) {
            return;
        }
        m_scene->clearGraph();
        if (m_undoStack) {
            m_undoStack->clear();
        }
        rebuildProjectTreeNodes();
        statusBar()->showMessage(QStringLiteral("Graph cleared"), 2000);
    });

    connect(deleteAction, &QAction::triggered, this, [this]() {
        if (m_scene) {
            m_scene->deleteSelectionWithUndo();
        }
    });

    connect(saveAction, &QAction::triggered, this, [this]() {
        if (!m_scene) {
            return;
        }

        QString path = m_currentFilePath;
        if (path.isEmpty()) {
            path = QFileDialog::getSaveFileName(
                this, QStringLiteral("Save Graph"), QStringLiteral("graph.eda.json"), QStringLiteral("EDA Graph (*.json)"));
        }
        if (path.isEmpty()) {
            return;
        }

        QString error;
        const GraphDocument document = m_scene->toDocument();
        if (!GraphSerializer::saveToFile(document, path, &error)) {
            QMessageBox::critical(this, QStringLiteral("Save Failed"), error);
            return;
        }
        m_currentFilePath = path;
        statusBar()->showMessage(QStringLiteral("Saved: %1").arg(path), 2500);
    });

    connect(openAction, &QAction::triggered, this, [this]() {
        if (!m_scene) {
            return;
        }
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("Open Graph"), QString(), QStringLiteral("EDA Graph (*.json)"));
        if (path.isEmpty()) {
            return;
        }

        GraphDocument document;
        QString error;
        if (!GraphSerializer::loadFromFile(&document, path, &error)) {
            QMessageBox::critical(this, QStringLiteral("Open Failed"), error);
            return;
        }
        if (!m_scene->fromDocument(document)) {
            QMessageBox::critical(this, QStringLiteral("Open Failed"), QStringLiteral("Graph rebuild failed."));
            return;
        }
        if (m_undoStack) {
            m_undoStack->clear();
        }
        m_currentFilePath = path;
        statusBar()->showMessage(QStringLiteral("Opened: %1").arg(path), 2500);
    });
}

void MainWindow::setupCentralArea() {
    m_editorTabs = new QTabWidget(this);
    m_editorTabs->setDocumentMode(true);
    m_editorTabs->setTabsClosable(true);
    m_editorTabs->setMovable(true);
    m_editorTabs->addTab(createEditorTab(QStringLiteral("RCP_SH1805")), QStringLiteral("RCP_SH1805"));
    m_editorTabs->addTab(createEditorTab(QStringLiteral("RCP_SH1208")), QStringLiteral("RCP_SH1208"));
    setCentralWidget(m_editorTabs);
    activateEditorTab(0);
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

    m_propertyTable = new QTableWidget(9, 2, m_propertyDock);
    m_propertyTable->setHorizontalHeaderLabels({QStringLiteral("Key"), QStringLiteral("Value")});
    m_propertyTable->horizontalHeader()->setStretchLastSection(true);
    m_propertyTable->verticalHeader()->setVisible(false);
    m_propertyTable->setAlternatingRowColors(true);
    m_propertyTable->setShowGrid(false);
    m_propertyTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
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
    const ComponentCatalog& catalog = ComponentCatalog::instance();
    const QStringList categories = catalog.categories();
    for (const QString& category : categories) {
        addPaletteCategory(toolBox, category, catalog.typesInCategory(category));
    }

    layout->addWidget(toolBox);
    m_paletteDock->setWidget(panel);
    addDockWidget(Qt::RightDockWidgetArea, m_paletteDock);
    m_viewMenu->addAction(m_paletteDock->toggleViewAction());
}

void MainWindow::setupSignalBindings() {
    for (int i = 0; i < m_views.size(); ++i) {
        GraphView* view = m_views[i];
        EditorScene* scene = m_scenes[i];

        connect(view, &GraphView::paletteItemDropped, this, [this, scene](const QString& typeName, const QPointF& scenePos) {
            NodeItem* node = scene->createNodeWithUndo(typeName, scenePos);
            if (scene == m_scene && node) {
                scene->clearSelection();
                node->setSelected(true);
                statusBar()->showMessage(QStringLiteral("Node created: %1").arg(typeName), 2000);
            }
        });

        connect(view, &GraphView::zoomChanged, this, [this, view](int percent) {
            if (view == m_graphView) {
                statusBar()->showMessage(QStringLiteral("Zoom: %1%").arg(percent), 1500);
            }
        });

        connect(scene,
                &EditorScene::selectionInfoChanged,
                this,
                [this, scene](const QString& itemType,
                              const QString& itemId,
                              const QString& displayName,
                              const QPointF& pos,
                              int inputCount,
                              int outputCount) {
                    if (scene == m_scene) {
                        updatePropertyTable(itemType, itemId, displayName, pos, inputCount, outputCount);
                    }
                });

        connect(scene, &EditorScene::graphChanged, this, [this, scene]() {
            if (scene == m_scene) {
                rebuildProjectTreeNodes();
            }
        });
    }

    connect(m_editorTabs, &QTabWidget::currentChanged, this, &MainWindow::activateEditorTab);
    connect(m_propertyTable, &QTableWidget::cellChanged, this, &MainWindow::onPropertyCellChanged);

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

QWidget* MainWindow::createEditorTab(const QString&) {
    EditorScene* scene = new EditorScene(this);
    scene->setSceneRect(0, 0, 3600, 2400);
    scene->setUndoStack(m_undoStack);

    GraphView* view = new GraphView(this);
    view->setScene(scene);

    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QLabel* rulerHint = new QLabel(QStringLiteral("Coordinate ruler placeholder"), page);
    rulerHint->setFixedHeight(20);
    rulerHint->setStyleSheet(
        "QLabel { color: #666; background: #f1f1f1; border-bottom: 1px solid #dadada; padding-left: 8px; }");
    layout->addWidget(rulerHint);
    layout->addWidget(view);

    m_scenes.push_back(scene);
    m_views.push_back(view);
    return page;
}

void MainWindow::activateEditorTab(int index) {
    if (index < 0 || index >= m_scenes.size()) {
        return;
    }
    m_scene = m_scenes[index];
    m_graphView = m_views[index];
    if (m_graphNodesRoot) {
        rebuildProjectTreeNodes();
    }
    if (m_propertyTable) {
        updatePropertyTable(QString(), QString(), QString(), QPointF(), 0, 0);
    }
    statusBar()->showMessage(QStringLiteral("Active tab: %1").arg(m_editorTabs->tabText(index)), 1200);
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
    m_propertyTableUpdating = true;
    const QSignalBlocker blocker(m_propertyTable);
    m_selectedItemType = itemType;
    m_selectedItemId = itemId;
    m_dynamicPropertyRows.clear();

    NodeItem* selectedNode = (itemType == QStringLiteral("node")) ? findNodeById(itemId) : nullptr;
    const QVector<PropertyData> customProps = selectedNode ? selectedNode->properties() : QVector<PropertyData>();

    const int baseRows = 9;
    m_propertyTable->clearContents();
    m_propertyTable->setRowCount(baseRows + customProps.size());

    auto setRow = [this](int row, const QString& key, const QString& val, bool editable) {
        auto* keyItem = new QTableWidgetItem(key);
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        m_propertyTable->setItem(row, 0, keyItem);

        auto* valueItem = new QTableWidgetItem(val);
        if (!editable) {
            valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
        }
        m_propertyTable->setItem(row, 1, valueItem);
    };

    const bool nodeEditable = (itemType == QStringLiteral("node"));
    setRow(0, QStringLiteral("Selection"), itemType.isEmpty() ? QStringLiteral("(none)") : itemType, false);
    setRow(1, QStringLiteral("ID"), itemId, false);
    setRow(2, QStringLiteral("Name"), displayName, nodeEditable);
    setRow(3, QStringLiteral("X"), QString::number(pos.x(), 'f', 1), nodeEditable);
    setRow(4, QStringLiteral("Y"), QString::number(pos.y(), 'f', 1), nodeEditable);
    setRow(5, QStringLiteral("Inputs"), QString::number(inputCount), false);
    setRow(6, QStringLiteral("Outputs"), QString::number(outputCount), false);

    auto* gridKey = new QTableWidgetItem(QStringLiteral("Grid Snap"));
    gridKey->setFlags(gridKey->flags() & ~Qt::ItemIsEditable);
    m_propertyTable->setItem(7, 0, gridKey);
    auto* gridCombo = new QComboBox(m_propertyTable);
    gridCombo->addItems({QStringLiteral("On"), QStringLiteral("Off")});
    gridCombo->setCurrentText(m_scene && m_scene->snapToGrid() ? QStringLiteral("On") : QStringLiteral("Off"));
    m_propertyTable->setCellWidget(7, 1, gridCombo);
    connect(gridCombo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        if (m_propertyTableUpdating || !m_scene) {
            return;
        }
        m_scene->setSnapToGrid(text == QStringLiteral("On"));
    });

    auto* routingKey = new QTableWidgetItem(QStringLiteral("Routing"));
    routingKey->setFlags(routingKey->flags() & ~Qt::ItemIsEditable);
    m_propertyTable->setItem(8, 0, routingKey);
    auto* routingCombo = new QComboBox(m_propertyTable);
    routingCombo->addItems({QStringLiteral("Manhattan"), QStringLiteral("Avoid Nodes")});
    const QString routingText =
        (m_scene && m_scene->edgeRoutingMode() == EdgeRoutingMode::ObstacleAvoiding) ? QStringLiteral("Avoid Nodes")
                                                                                       : QStringLiteral("Manhattan");
    routingCombo->setCurrentText(routingText);
    m_propertyTable->setCellWidget(8, 1, routingCombo);
    connect(routingCombo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        if (m_propertyTableUpdating || !m_scene) {
            return;
        }
        const EdgeRoutingMode mode =
            (text == QStringLiteral("Avoid Nodes")) ? EdgeRoutingMode::ObstacleAvoiding : EdgeRoutingMode::Manhattan;
        m_scene->setEdgeRoutingMode(mode);
    });

    for (int i = 0; i < customProps.size(); ++i) {
        const int row = baseRows + i;
        const PropertyData& prop = customProps[i];
        m_dynamicPropertyRows.insert(row, prop);

        auto* keyItem = new QTableWidgetItem(prop.key);
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        m_propertyTable->setItem(row, 0, keyItem);

        if (prop.type == QStringLiteral("bool")) {
            auto* combo = new QComboBox(m_propertyTable);
            combo->addItems({QStringLiteral("true"), QStringLiteral("false")});
            combo->setCurrentText(prop.value.toLower() == QStringLiteral("false") ? QStringLiteral("false") : QStringLiteral("true"));
            m_propertyTable->setCellWidget(row, 1, combo);
            connect(combo, &QComboBox::currentTextChanged, this, [this, row](const QString& text) {
                if (m_propertyTableUpdating || !m_scene || m_selectedItemType != QStringLiteral("node")) {
                    return;
                }
                const PropertyData prop = m_dynamicPropertyRows.value(row);
                m_scene->setNodePropertyWithUndo(m_selectedItemId, prop.key, text);
            });
        } else if (prop.type == QStringLiteral("int")) {
            auto* spin = new QSpinBox(m_propertyTable);
            spin->setRange(-1000000, 1000000);
            spin->setValue(prop.value.toInt());
            m_propertyTable->setCellWidget(row, 1, spin);
            connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, [this, row](int v) {
                if (m_propertyTableUpdating || !m_scene || m_selectedItemType != QStringLiteral("node")) {
                    return;
                }
                const PropertyData prop = m_dynamicPropertyRows.value(row);
                m_scene->setNodePropertyWithUndo(m_selectedItemId, prop.key, QString::number(v));
            });
        } else if (prop.type == QStringLiteral("double")) {
            auto* spin = new QDoubleSpinBox(m_propertyTable);
            spin->setDecimals(4);
            spin->setRange(-1.0e9, 1.0e9);
            spin->setValue(prop.value.toDouble());
            m_propertyTable->setCellWidget(row, 1, spin);
            connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, row](double v) {
                if (m_propertyTableUpdating || !m_scene || m_selectedItemType != QStringLiteral("node")) {
                    return;
                }
                const PropertyData prop = m_dynamicPropertyRows.value(row);
                m_scene->setNodePropertyWithUndo(m_selectedItemId, prop.key, QString::number(v, 'f', 4));
            });
        } else {
            auto* item = new QTableWidgetItem(prop.value);
            m_propertyTable->setItem(row, 1, item);
        }
    }

    if (itemType == QStringLiteral("node")) {
        if (QTreeWidgetItem* item = findTreeItemByNodeId(itemId)) {
            const QSignalBlocker treeBlocker(m_projectTree);
            m_projectTree->setCurrentItem(item);
        }
    }
    m_propertyTableUpdating = false;
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

void MainWindow::onPropertyCellChanged(int row, int column) {
    if (!m_scene || column != 1 || m_propertyTableUpdating) {
        return;
    }
    QTableWidgetItem* valueItem = m_propertyTable->item(row, column);
    if (m_selectedItemType != QStringLiteral("node") || m_selectedItemId.isEmpty()) {
        return;
    }

    if (m_dynamicPropertyRows.contains(row)) {
        const PropertyData prop = m_dynamicPropertyRows.value(row);
        if (prop.type == QStringLiteral("string") && valueItem) {
            m_scene->setNodePropertyWithUndo(m_selectedItemId, prop.key, valueItem->text().trimmed());
        }
        return;
    }

    if (!valueItem) {
        return;
    }
    const QString value = valueItem->text().trimmed();

    if (row == 2) {
        if (m_scene->renameNodeWithUndo(m_selectedItemId, value)) {
            rebuildProjectTreeNodes();
        }
        return;
    }

    if (row == 3 || row == 4) {
        bool okX = false;
        bool okY = false;
        const double x = m_propertyTable->item(3, 1)->text().toDouble(&okX);
        const double y = m_propertyTable->item(4, 1)->text().toDouble(&okY);
        if (okX && okY) {
            m_scene->moveNodeWithUndo(m_selectedItemId, QPointF(x, y));
        }
    }
}

NodeItem* MainWindow::findNodeById(const QString& nodeId) const {
    if (!m_scene || nodeId.isEmpty()) {
        return nullptr;
    }
    for (QGraphicsItem* item : m_scene->items()) {
        if (NodeItem* node = dynamic_cast<NodeItem*>(item)) {
            if (node->nodeId() == nodeId) {
                return node;
            }
        }
    }
    return nullptr;
}

QTreeWidgetItem* MainWindow::findTreeItemByNodeId(const QString& nodeId) const {
    if (!m_graphNodesRoot || nodeId.isEmpty()) {
        return nullptr;
    }
    for (int i = 0; i < m_graphNodesRoot->childCount(); ++i) {
        QTreeWidgetItem* child = m_graphNodesRoot->child(i);
        if (child && child->data(0, Qt::UserRole).toString() == nodeId) {
            return child;
        }
    }
    return nullptr;
}
