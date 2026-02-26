#include "MainWindow.h"

#include "GraphView.h"
#include "items/EdgeItem.h"
#include "items/NodeItem.h"
#include "model/GraphSerializer.h"
#include "panels/PalettePanel.h"
#include "panels/ProjectTreePanel.h"
#include "panels/PropertyPanel.h"
#include "scene/EditorScene.h"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyle>
#include <QTableWidget>
#include <QTabWidget>
#include <QToolBar>
#include <QUndoGroup>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QtGlobal>

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

int MainWindow::documentCount() const {
    return m_documents.size();
}

int MainWindow::activeDocumentIndex() const {
    return currentDocumentIndex();
}

bool MainWindow::isDocumentDirty(int index) const {
    if (index < 0 || index >= m_documents.size()) {
        return false;
    }
    return m_documents[index].dirty;
}

QString MainWindow::documentFilePath(int index) const {
    if (index < 0 || index >= m_documents.size()) {
        return QString();
    }
    return m_documents[index].filePath;
}

EditorScene* MainWindow::activeScene() const {
    return m_scene;
}

int MainWindow::newDocument(const QString& title) {
    const QString resolvedTitle = title.isEmpty() ? QStringLiteral("Untitled-%1").arg(m_untitledCounter++) : title;
    const int index = createEditorTab(resolvedTitle);
    if (index >= 0) {
        m_editorTabs->setCurrentIndex(index);
    }
    return index;
}

bool MainWindow::openDocumentByDialog() {
    const QString path = requestOpenFilePath();
    if (path.isEmpty()) {
        return false;
    }
    return openDocumentFromPath(path);
}

bool MainWindow::openDocumentFromPath(const QString& path) {
    if (path.isEmpty()) {
        return false;
    }

    GraphDocument document;
    QString error;
    if (!GraphSerializer::loadFromFile(&document, path, &error)) {
        showCriticalMessage(QStringLiteral("Open Failed"), error);
        return false;
    }

    const QString title = QFileInfo(path).fileName();
    const int index = createEditorTab(title, &document, path);
    if (index < 0) {
        return false;
    }

    m_editorTabs->setCurrentIndex(index);
    statusBar()->showMessage(QStringLiteral("Opened: %1").arg(path), 2500);
    return true;
}

bool MainWindow::saveCurrentDocument(bool saveAs) {
    const int index = currentDocumentIndex();
    if (index < 0) {
        return false;
    }
    return saveDocument(index, saveAs);
}

bool MainWindow::closeDocument(int index) {
    const int resolvedIndex = (index >= 0) ? index : currentDocumentIndex();
    if (resolvedIndex < 0) {
        return false;
    }
    return closeDocumentTab(resolvedIndex);
}

void MainWindow::setOpenFileDialogProvider(const std::function<QString()>& provider) {
    m_openFileDialogProvider = provider;
}

void MainWindow::setSaveFileDialogProvider(const std::function<QString(const QString& suggested)>& provider) {
    m_saveFileDialogProvider = provider;
}

void MainWindow::setUnsavedPromptProvider(
    const std::function<QMessageBox::StandardButton(const QString& docTitle)>& provider) {
    m_unsavedPromptProvider = provider;
}

void MainWindow::setCriticalMessageProvider(const std::function<void(const QString& title, const QString& text)>& provider) {
    m_criticalMessageProvider = provider;
}

void MainWindow::clearDialogProviders() {
    m_openFileDialogProvider = nullptr;
    m_saveFileDialogProvider = nullptr;
    m_unsavedPromptProvider = nullptr;
    m_criticalMessageProvider = nullptr;
}

void MainWindow::setupWindow() {
    resize(1600, 940);
    setWindowTitle(QStringLiteral("EDA Editor Prototype"));
    setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks | QMainWindow::AnimatedDocks);
    statusBar()->showMessage(QStringLiteral("Ready"));
}

void MainWindow::setupMenusAndToolbar() {
    m_undoGroup = new QUndoGroup(this);

    QMenu* projectMenu = menuBar()->addMenu(QStringLiteral("Project"));
    QMenu* editMenu = menuBar()->addMenu(QStringLiteral("Edit"));
    m_viewMenu = menuBar()->addMenu(QStringLiteral("View"));
    QMenu* runMenu = menuBar()->addMenu(QStringLiteral("Run"));
    menuBar()->addMenu(QStringLiteral("Help"));

    QAction* newAction =
        projectMenu->addAction(style()->standardIcon(QStyle::SP_FileIcon), QStringLiteral("New"));
    newAction->setShortcut(QKeySequence::New);

    QAction* openAction =
        projectMenu->addAction(style()->standardIcon(QStyle::SP_DialogOpenButton), QStringLiteral("Open"));
    openAction->setShortcut(QKeySequence::Open);

    m_saveAction =
        projectMenu->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton), QStringLiteral("Save"));
    m_saveAction->setShortcut(QKeySequence::Save);

    m_saveAsAction = projectMenu->addAction(QStringLiteral("Save As"));
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);

    QAction* closeTabAction = projectMenu->addAction(QStringLiteral("Close Tab"));
    closeTabAction->setShortcut(QKeySequence::Close);

    QAction* clearAction = projectMenu->addAction(QStringLiteral("Clear Graph"));
    projectMenu->addSeparator();
    projectMenu->addAction(QStringLiteral("Exit"), this, &QWidget::close);

    QAction* undoAction = m_undoGroup->createUndoAction(this, QStringLiteral("Undo"));
    undoAction->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    QAction* redoAction = m_undoGroup->createRedoAction(this, QStringLiteral("Redo"));
    redoAction->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    editMenu->addAction(undoAction);
    editMenu->addAction(redoAction);
    editMenu->addSeparator();
    QAction* deleteAction = editMenu->addAction(QStringLiteral("Delete"));
    deleteAction->setShortcut(QKeySequence::Delete);
    QAction* autoLayoutAction = editMenu->addAction(QStringLiteral("Auto Layout"));
    autoLayoutAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+L")));
    editMenu->addSeparator();
    QAction* groupAction = editMenu->addAction(QStringLiteral("Group"));
    groupAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+G")));
    QAction* ungroupAction = editMenu->addAction(QStringLiteral("Ungroup"));
    ungroupAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+G")));
    QAction* collapseGroupAction = editMenu->addAction(QStringLiteral("Collapse Group"));
    collapseGroupAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+-")));
    QAction* expandGroupAction = editMenu->addAction(QStringLiteral("Expand Group"));
    expandGroupAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+=")));
    editMenu->addSeparator();
    QAction* rotateCwAction = editMenu->addAction(QStringLiteral("Rotate +90"));
    rotateCwAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    QAction* rotateCcwAction = editMenu->addAction(QStringLiteral("Rotate -90"));
    rotateCcwAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+R")));
    editMenu->addSeparator();
    QAction* bringFrontAction = editMenu->addAction(QStringLiteral("Bring To Front"));
    bringFrontAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+]")));
    QAction* sendBackAction = editMenu->addAction(QStringLiteral("Send To Back"));
    sendBackAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+[")));
    QAction* bringForwardAction = editMenu->addAction(QStringLiteral("Bring Forward"));
    QAction* sendBackwardAction = editMenu->addAction(QStringLiteral("Send Backward"));

    runMenu->addAction(style()->standardIcon(QStyle::SP_MediaPlay), QStringLiteral("Run"));
    runMenu->addAction(style()->standardIcon(QStyle::SP_MediaStop), QStringLiteral("Stop"));

    QToolBar* toolBar = addToolBar(QStringLiteral("Main"));
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(20, 20));
    toolBar->addAction(newAction);
    toolBar->addAction(openAction);
    toolBar->addAction(m_saveAction);
    toolBar->addSeparator();
    toolBar->addAction(undoAction);
    toolBar->addAction(redoAction);
    toolBar->addSeparator();
    toolBar->addAction(clearAction);
    toolBar->addAction(autoLayoutAction);
    toolBar->addAction(groupAction);
    toolBar->addAction(ungroupAction);
    toolBar->addAction(collapseGroupAction);
    toolBar->addAction(expandGroupAction);
    toolBar->addAction(rotateCwAction);
    toolBar->addAction(rotateCcwAction);
    toolBar->addSeparator();
    toolBar->addAction(style()->standardIcon(QStyle::SP_MediaPlay), QStringLiteral("Run"));

    connect(newAction, &QAction::triggered, this, [this]() {
        newDocument();
    });

    connect(openAction, &QAction::triggered, this, [this]() {
        openDocumentByDialog();
    });

    connect(m_saveAction, &QAction::triggered, this, [this]() {
        saveCurrentDocument(false);
    });

    connect(m_saveAsAction, &QAction::triggered, this, [this]() {
        saveCurrentDocument(true);
    });

    connect(closeTabAction, &QAction::triggered, this, [this]() {
        closeDocument();
    });

    connect(clearAction, &QAction::triggered, this, [this]() {
        const int index = currentDocumentIndex();
        if (index < 0 || !m_scene) {
            return;
        }
        m_scene->clearGraph();
        if (m_documents[index].undoStack) {
            m_documents[index].undoStack->clear();
        }
        setDocumentDirty(index, true);
        rebuildProjectTreeNodes();
        statusBar()->showMessage(QStringLiteral("Graph cleared"), 2000);
    });

    connect(deleteAction, &QAction::triggered, this, [this]() {
        if (m_scene) {
            m_scene->deleteSelectionWithUndo();
        }
    });

    connect(autoLayoutAction, &QAction::triggered, this, [this]() {
        if (!m_scene) {
            return;
        }
        if (m_scene->autoLayoutWithUndo(true)) {
            rebuildProjectTreeNodes();
            statusBar()->showMessage(QStringLiteral("Auto layout applied"), 2000);
            return;
        }
        statusBar()->showMessage(QStringLiteral("Auto layout skipped"), 1200);
    });

    connect(groupAction, &QAction::triggered, this, [this]() {
        if (!m_scene) {
            return;
        }
        if (m_scene->groupSelectionWithUndo()) {
            statusBar()->showMessage(QStringLiteral("Group created"), 1200);
        } else {
            statusBar()->showMessage(QStringLiteral("Group skipped"), 1200);
        }
    });

    connect(ungroupAction, &QAction::triggered, this, [this]() {
        if (!m_scene) {
            return;
        }
        if (m_scene->ungroupSelectionWithUndo()) {
            statusBar()->showMessage(QStringLiteral("Group removed"), 1200);
        } else {
            statusBar()->showMessage(QStringLiteral("Ungroup skipped"), 1200);
        }
    });

    connect(collapseGroupAction, &QAction::triggered, this, [this]() {
        if (!m_scene) {
            return;
        }
        if (m_scene->collapseSelectionWithUndo()) {
            statusBar()->showMessage(QStringLiteral("Group collapsed"), 1200);
        } else {
            statusBar()->showMessage(QStringLiteral("Collapse skipped"), 1200);
        }
    });

    connect(expandGroupAction, &QAction::triggered, this, [this]() {
        if (!m_scene) {
            return;
        }
        if (m_scene->expandSelectionWithUndo()) {
            statusBar()->showMessage(QStringLiteral("Group expanded"), 1200);
        } else {
            statusBar()->showMessage(QStringLiteral("Expand skipped"), 1200);
        }
    });

    connect(rotateCwAction, &QAction::triggered, this, [this]() {
        if (!m_scene) {
            return;
        }
        if (m_scene->rotateSelectionWithUndo(90.0)) {
            statusBar()->showMessage(QStringLiteral("Rotate +90"), 1200);
        } else {
            statusBar()->showMessage(QStringLiteral("Rotate skipped"), 1200);
        }
    });

    connect(rotateCcwAction, &QAction::triggered, this, [this]() {
        if (!m_scene) {
            return;
        }
        if (m_scene->rotateSelectionWithUndo(-90.0)) {
            statusBar()->showMessage(QStringLiteral("Rotate -90"), 1200);
        } else {
            statusBar()->showMessage(QStringLiteral("Rotate skipped"), 1200);
        }
    });

    connect(bringFrontAction, &QAction::triggered, this, [this]() {
        if (!m_scene) {
            return;
        }
        if (m_scene->bringSelectionToFrontWithUndo()) {
            statusBar()->showMessage(QStringLiteral("Bring to front"), 1200);
        } else {
            statusBar()->showMessage(QStringLiteral("Layer operation skipped"), 1200);
        }
    });

    connect(sendBackAction, &QAction::triggered, this, [this]() {
        if (!m_scene) {
            return;
        }
        if (m_scene->sendSelectionToBackWithUndo()) {
            statusBar()->showMessage(QStringLiteral("Send to back"), 1200);
        } else {
            statusBar()->showMessage(QStringLiteral("Layer operation skipped"), 1200);
        }
    });

    connect(bringForwardAction, &QAction::triggered, this, [this]() {
        if (!m_scene) {
            return;
        }
        if (m_scene->bringSelectionForwardWithUndo()) {
            statusBar()->showMessage(QStringLiteral("Bring forward"), 1200);
        } else {
            statusBar()->showMessage(QStringLiteral("Layer operation skipped"), 1200);
        }
    });

    connect(sendBackwardAction, &QAction::triggered, this, [this]() {
        if (!m_scene) {
            return;
        }
        if (m_scene->sendSelectionBackwardWithUndo()) {
            statusBar()->showMessage(QStringLiteral("Send backward"), 1200);
        } else {
            statusBar()->showMessage(QStringLiteral("Layer operation skipped"), 1200);
        }
    });
}

void MainWindow::setupCentralArea() {
    m_editorTabs = new QTabWidget(this);
    m_editorTabs->setDocumentMode(true);
    m_editorTabs->setTabsClosable(true);
    m_editorTabs->setMovable(true);
    setCentralWidget(m_editorTabs);

    const int index = createEditorTab(QStringLiteral("Untitled-%1").arg(m_untitledCounter++));
    if (index >= 0) {
        m_editorTabs->setCurrentIndex(index);
        activateEditorTab(index);
    }
}

void MainWindow::setupLeftDocks() {
    m_projectDock = new QDockWidget(QStringLiteral("Project"), this);
    m_projectDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_projectDock->setMinimumWidth(230);
    m_projectPanel = new ProjectTreePanel(m_projectDock);
    m_projectDock->setWidget(m_projectPanel);
    addDockWidget(Qt::LeftDockWidgetArea, m_projectDock);

    m_propertyDock = new QDockWidget(QStringLiteral("Properties"), this);
    m_propertyDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_propertyDock->setMinimumHeight(220);
    m_propertyPanel = new PropertyPanel(m_propertyDock);
    m_propertyTable = m_propertyPanel->table();
    m_propertyDock->setWidget(m_propertyPanel);
    addDockWidget(Qt::LeftDockWidgetArea, m_propertyDock);

    splitDockWidget(m_projectDock, m_propertyDock, Qt::Vertical);

    m_viewMenu->addAction(m_projectDock->toggleViewAction());
    m_viewMenu->addAction(m_propertyDock->toggleViewAction());
}

void MainWindow::setupRightDock() {
    m_paletteDock = new QDockWidget(QStringLiteral("Toolbox"), this);
    m_paletteDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_paletteDock->setMinimumWidth(280);
    m_palettePanel = new PalettePanel(m_paletteDock);
    m_paletteDock->setWidget(m_palettePanel);
    addDockWidget(Qt::RightDockWidgetArea, m_paletteDock);
    m_viewMenu->addAction(m_paletteDock->toggleViewAction());
}

void MainWindow::setupSignalBindings() {
    connect(m_editorTabs, &QTabWidget::currentChanged, this, &MainWindow::activateEditorTab);
    connect(m_editorTabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeDocumentTab);
    connect(m_propertyTable, &QTableWidget::cellChanged, this, &MainWindow::onPropertyCellChanged);
    connect(m_projectPanel, &ProjectTreePanel::nodeSelected, this, [this](const QString& nodeId) {
        if (!m_scene || nodeId.isEmpty()) {
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
    const int index = currentDocumentIndex();
    if (index < 0 || !m_scene) {
        return;
    }

    DocumentContext& doc = m_documents[index];
    doc.suppressDirtyTracking = true;

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

    doc.suppressDirtyTracking = false;
    if (doc.undoStack) {
        doc.undoStack->clear();
    }
    setDocumentDirty(index, false);
}

int MainWindow::createEditorTab(const QString& title, const GraphDocument* initialDocument, const QString& filePath) {
    EditorScene* scene = new EditorScene(this);
    scene->setSceneRect(0, 0, 3600, 2400);

    QUndoStack* undoStack = new QUndoStack(this);
    scene->setUndoStack(undoStack);
    if (m_undoGroup) {
        m_undoGroup->addStack(undoStack);
    }

    if (initialDocument && !scene->fromDocument(*initialDocument)) {
        if (m_undoGroup) {
            m_undoGroup->removeStack(undoStack);
        }
        delete undoStack;
        delete scene;
        showCriticalMessage(QStringLiteral("Open Failed"), QStringLiteral("Graph rebuild failed."));
        return -1;
    }

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

    const int index = m_documents.size();
    m_documents.push_back(DocumentContext{scene, view, undoStack, title, filePath, false, false});
    m_editorTabs->addTab(page, title);

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
        const int i = documentIndexForScene(scene);
        if (i < 0) {
            return;
        }
        if (!m_documents[i].suppressDirtyTracking) {
            setDocumentDirty(i, true);
        }
        if (scene == m_scene) {
            rebuildProjectTreeNodes();
        }
    });

    connect(undoStack, &QUndoStack::cleanChanged, this, [this, undoStack](bool clean) {
        const int i = documentIndexForUndoStack(undoStack);
        if (i < 0 || m_documents[i].suppressDirtyTracking) {
            return;
        }
        setDocumentDirty(i, !clean);
    });

    return index;
}

void MainWindow::activateEditorTab(int index) {
    if (index < 0 || index >= m_documents.size()) {
        m_scene = nullptr;
        m_graphView = nullptr;
        if (m_undoGroup) {
            m_undoGroup->setActiveStack(nullptr);
        }
        if (m_propertyTable) {
            updatePropertyTable(QString(), QString(), QString(), QPointF(), 0, 0);
        }
        return;
    }

    m_scene = m_documents[index].scene;
    m_graphView = m_documents[index].view;
    if (m_undoGroup) {
        m_undoGroup->setActiveStack(m_documents[index].undoStack);
    }
    if (m_projectPanel) {
        rebuildProjectTreeNodes();
    }
    if (m_propertyTable) {
        updatePropertyTable(QString(), QString(), QString(), QPointF(), 0, 0);
    }
    statusBar()->showMessage(QStringLiteral("Active tab: %1").arg(m_documents[index].title), 1200);
}

int MainWindow::documentIndexForScene(const EditorScene* scene) const {
    if (!scene) {
        return -1;
    }
    for (int i = 0; i < m_documents.size(); ++i) {
        if (m_documents[i].scene == scene) {
            return i;
        }
    }
    return -1;
}

int MainWindow::documentIndexForUndoStack(const QUndoStack* stack) const {
    if (!stack) {
        return -1;
    }
    for (int i = 0; i < m_documents.size(); ++i) {
        if (m_documents[i].undoStack == stack) {
            return i;
        }
    }
    return -1;
}

int MainWindow::currentDocumentIndex() const {
    return m_editorTabs ? m_editorTabs->currentIndex() : -1;
}

QString MainWindow::requestOpenFilePath() const {
    if (m_openFileDialogProvider) {
        return m_openFileDialogProvider();
    }
    return QFileDialog::getOpenFileName(
        const_cast<MainWindow*>(this), QStringLiteral("Open Graph"), QString(), QStringLiteral("EDA Graph (*.json)"));
}

QString MainWindow::requestSaveFilePath(const QString& suggested) const {
    if (m_saveFileDialogProvider) {
        return m_saveFileDialogProvider(suggested);
    }
    return QFileDialog::getSaveFileName(
        const_cast<MainWindow*>(this), QStringLiteral("Save Graph"), suggested, QStringLiteral("EDA Graph (*.json)"));
}

QMessageBox::StandardButton MainWindow::requestUnsavedDecision(const QString& docTitle) const {
    if (m_unsavedPromptProvider) {
        return m_unsavedPromptProvider(docTitle);
    }
    return QMessageBox::warning(const_cast<MainWindow*>(this),
                                QStringLiteral("Unsaved Changes"),
                                QStringLiteral("Document \"%1\" has unsaved changes. Save before closing?").arg(docTitle),
                                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                                QMessageBox::Save);
}

void MainWindow::showCriticalMessage(const QString& title, const QString& text) const {
    if (m_criticalMessageProvider) {
        m_criticalMessageProvider(title, text);
        return;
    }
    QMessageBox::critical(const_cast<MainWindow*>(this), title, text);
}

bool MainWindow::saveDocument(int index, bool saveAs) {
    if (index < 0 || index >= m_documents.size()) {
        return false;
    }

    DocumentContext& doc = m_documents[index];
    QString path = doc.filePath;
    if (saveAs || path.isEmpty()) {
        const QString suggested = path.isEmpty()
            ? QStringLiteral("%1.eda.json").arg(doc.title.isEmpty() ? QStringLiteral("graph") : doc.title)
            : path;
        path = requestSaveFilePath(suggested);
    }
    if (path.isEmpty()) {
        return false;
    }

    QString error;
    const GraphDocument document = doc.scene->toDocument();
    if (!GraphSerializer::saveToFile(document, path, &error)) {
        showCriticalMessage(QStringLiteral("Save Failed"), error);
        return false;
    }

    doc.filePath = path;
    doc.title = QFileInfo(path).fileName();
    setDocumentDirty(index, false);
    if (doc.undoStack) {
        doc.undoStack->setClean();
    }

    statusBar()->showMessage(QStringLiteral("Saved: %1").arg(path), 2500);
    return true;
}

bool MainWindow::maybeSaveDocument(int index) {
    if (index < 0 || index >= m_documents.size()) {
        return true;
    }

    const DocumentContext& doc = m_documents[index];
    if (!doc.dirty) {
        return true;
    }

    const QMessageBox::StandardButton choice = requestUnsavedDecision(doc.title);

    if (choice == QMessageBox::Save) {
        return saveDocument(index, false);
    }
    if (choice == QMessageBox::Discard) {
        return true;
    }
    return false;
}

bool MainWindow::closeDocumentTab(int index) {
    if (index < 0 || index >= m_documents.size()) {
        return false;
    }

    if (!maybeSaveDocument(index)) {
        return false;
    }

    const DocumentContext doc = m_documents[index];
    QWidget* page = m_editorTabs->widget(index);
    m_documents.removeAt(index);
    m_editorTabs->removeTab(index);

    if (m_undoGroup && doc.undoStack) {
        m_undoGroup->removeStack(doc.undoStack);
    }
    delete doc.undoStack;
    delete doc.scene;
    delete page;

    if (m_documents.isEmpty()) {
        const int newIndex = createEditorTab(QStringLiteral("Untitled-%1").arg(m_untitledCounter++));
        if (newIndex >= 0) {
            m_editorTabs->setCurrentIndex(newIndex);
        }
    }

    activateEditorTab(currentDocumentIndex());
    return true;
}

void MainWindow::setDocumentDirty(int index, bool dirty) {
    if (index < 0 || index >= m_documents.size()) {
        return;
    }
    if (m_documents[index].dirty == dirty) {
        return;
    }
    m_documents[index].dirty = dirty;
    updateTabTitle(index);
}

void MainWindow::updateTabTitle(int index) {
    if (!m_editorTabs || index < 0 || index >= m_documents.size()) {
        return;
    }
    const DocumentContext& doc = m_documents[index];
    const QString tabText = doc.dirty ? doc.title + QStringLiteral("*") : doc.title;
    m_editorTabs->setTabText(index, tabText);
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

    const int baseRows = 12;
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

    auto* autoLayoutModeKey = new QTableWidgetItem(QStringLiteral("Auto Layout Mode"));
    autoLayoutModeKey->setFlags(autoLayoutModeKey->flags() & ~Qt::ItemIsEditable);
    m_propertyTable->setItem(9, 0, autoLayoutModeKey);
    auto* autoLayoutModeCombo = new QComboBox(m_propertyTable);
    autoLayoutModeCombo->addItems({QStringLiteral("Layered"), QStringLiteral("Grid")});
    const QString autoLayoutModeText = (m_scene && m_scene->autoLayoutMode() == AutoLayoutMode::Grid)
                                           ? QStringLiteral("Grid")
                                           : QStringLiteral("Layered");
    autoLayoutModeCombo->setCurrentText(autoLayoutModeText);
    m_propertyTable->setCellWidget(9, 1, autoLayoutModeCombo);
    connect(autoLayoutModeCombo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        if (m_propertyTableUpdating || !m_scene) {
            return;
        }
        const AutoLayoutMode mode = (text == QStringLiteral("Grid")) ? AutoLayoutMode::Grid : AutoLayoutMode::Layered;
        m_scene->setAutoLayoutMode(mode);
    });

    auto* layoutXKey = new QTableWidgetItem(QStringLiteral("Layout X Spacing"));
    layoutXKey->setFlags(layoutXKey->flags() & ~Qt::ItemIsEditable);
    m_propertyTable->setItem(10, 0, layoutXKey);
    auto* layoutXSpin = new QSpinBox(m_propertyTable);
    layoutXSpin->setRange(40, 2000);
    layoutXSpin->setValue(m_scene ? qRound(m_scene->autoLayoutHorizontalSpacing()) : 240);
    m_propertyTable->setCellWidget(10, 1, layoutXSpin);
    connect(layoutXSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        if (m_propertyTableUpdating || !m_scene) {
            return;
        }
        m_scene->setAutoLayoutSpacing(static_cast<qreal>(value), m_scene->autoLayoutVerticalSpacing());
    });

    auto* layoutYKey = new QTableWidgetItem(QStringLiteral("Layout Y Spacing"));
    layoutYKey->setFlags(layoutYKey->flags() & ~Qt::ItemIsEditable);
    m_propertyTable->setItem(11, 0, layoutYKey);
    auto* layoutYSpin = new QSpinBox(m_propertyTable);
    layoutYSpin->setRange(40, 2000);
    layoutYSpin->setValue(m_scene ? qRound(m_scene->autoLayoutVerticalSpacing()) : 140);
    m_propertyTable->setCellWidget(11, 1, layoutYSpin);
    connect(layoutYSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        if (m_propertyTableUpdating || !m_scene) {
            return;
        }
        m_scene->setAutoLayoutSpacing(m_scene->autoLayoutHorizontalSpacing(), static_cast<qreal>(value));
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
        if (m_projectPanel) {
            m_projectPanel->selectNode(itemId);
        }
    }
    m_propertyTableUpdating = false;
}

void MainWindow::rebuildProjectTreeNodes() {
    if (!m_projectPanel) {
        return;
    }
    m_projectPanel->rebuildFromScene(m_scene);
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

void MainWindow::closeEvent(QCloseEvent* event) {
    for (int i = m_documents.size() - 1; i >= 0; --i) {
        if (!maybeSaveDocument(i)) {
            event->ignore();
            return;
        }
    }
    event->accept();
}
