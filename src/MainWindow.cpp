#include "MainWindow.h"

#include "GraphView.h"

#include <QDockWidget>
#include <QFrame>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPainterPath>
#include <QStatusBar>
#include <QStyle>
#include <QTableWidget>
#include <QTabWidget>
#include <QToolBar>
#include <QToolBox>
#include <QTreeWidget>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setupWindow();
    setupMenusAndToolbar();
    setupCentralArea();
    setupLeftDocks();
    setupRightDock();
    populateDemoGraph();
}

void MainWindow::setupWindow() {
    resize(1600, 940);
    setWindowTitle(QStringLiteral("EDA Editor UI Prototype"));
    setDockOptions(QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks | QMainWindow::AnimatedDocks);
    statusBar()->showMessage(QStringLiteral("就绪"));
}

void MainWindow::setupMenusAndToolbar() {
    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("项目"));
    QMenu* editMenu = menuBar()->addMenu(QStringLiteral("编辑"));
    m_viewMenu = menuBar()->addMenu(QStringLiteral("视图"));
    QMenu* runMenu = menuBar()->addMenu(QStringLiteral("执行"));
    menuBar()->addMenu(QStringLiteral("帮助"));

    QAction* saveAction = fileMenu->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton), QStringLiteral("保存项目"));
    fileMenu->addAction(style()->standardIcon(QStyle::SP_DialogOpenButton), QStringLiteral("打开项目"));
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("退出"), this, &QWidget::close);

    editMenu->addAction(style()->standardIcon(QStyle::SP_ArrowBack), QStringLiteral("撤销"));
    editMenu->addAction(style()->standardIcon(QStyle::SP_ArrowForward), QStringLiteral("重做"));
    editMenu->addSeparator();
    editMenu->addAction(QStringLiteral("删除"));

    runMenu->addAction(style()->standardIcon(QStyle::SP_MediaPlay), QStringLiteral("运行"));
    runMenu->addAction(style()->standardIcon(QStyle::SP_MediaStop), QStringLiteral("停止"));

    QToolBar* toolBar = addToolBar(QStringLiteral("主工具栏"));
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(20, 20));

    toolBar->addAction(saveAction);
    toolBar->addSeparator();
    toolBar->addAction(style()->standardIcon(QStyle::SP_ArrowBack), QStringLiteral("撤销"));
    toolBar->addAction(style()->standardIcon(QStyle::SP_ArrowForward), QStringLiteral("重做"));
    toolBar->addSeparator();
    toolBar->addAction(style()->standardIcon(QStyle::SP_FileDialogDetailedView), QStringLiteral("缩放到适应"));
    toolBar->addAction(style()->standardIcon(QStyle::SP_TitleBarMaxButton), QStringLiteral("1:1"));
    toolBar->addSeparator();
    toolBar->addAction(style()->standardIcon(QStyle::SP_MediaPlay), QStringLiteral("运行"));
}

void MainWindow::setupCentralArea() {
    m_editorTabs = new QTabWidget(this);
    m_editorTabs->setDocumentMode(true);
    m_editorTabs->setTabsClosable(true);

    m_scene = new QGraphicsScene(this);
    m_scene->setSceneRect(0, 0, 3600, 2400);

    m_graphView = new GraphView(this);
    m_graphView->setScene(m_scene);

    QWidget* editorPage = new QWidget(this);
    QVBoxLayout* pageLayout = new QVBoxLayout(editorPage);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    QLabel* rulerHint = new QLabel(QStringLiteral("横纵坐标尺（原型占位）"), editorPage);
    rulerHint->setFixedHeight(20);
    rulerHint->setStyleSheet("QLabel { color: #666; background: #f1f1f1; border-bottom: 1px solid #dadada; padding-left: 8px; }");
    pageLayout->addWidget(rulerHint);
    pageLayout->addWidget(m_graphView);

    m_editorTabs->addTab(editorPage, QStringLiteral("RCP_SH1805"));
    m_editorTabs->addTab(new QLabel(QStringLiteral("预留标签页"), this), QStringLiteral("RCP_SH1208"));
    setCentralWidget(m_editorTabs);
}

void MainWindow::setupLeftDocks() {
    m_projectDock = new QDockWidget(QStringLiteral("项目面板"), this);
    m_projectDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_projectDock->setMinimumWidth(220);

    m_projectTree = new QTreeWidget(m_projectDock);
    m_projectTree->setHeaderHidden(true);
    auto* root = new QTreeWidgetItem(QStringList() << QStringLiteral("RCP_SH0005"));
    const QStringList children = {
        QStringLiteral("RCP_SH0009"),
        QStringLiteral("RCP_SH0010"),
        QStringLiteral("RCP_SH1102"),
        QStringLiteral("RCP_SH1103"),
        QStringLiteral("RCP_SH1207"),
        QStringLiteral("RCP_SH1212"),
        QStringLiteral("RCP_SH1804"),
        QStringLiteral("RCP_SH1805")
    };
    for (const QString& name : children) {
        root->addChild(new QTreeWidgetItem(QStringList() << name));
    }
    m_projectTree->addTopLevelItem(root);
    m_projectTree->expandAll();
    m_projectDock->setWidget(m_projectTree);
    addDockWidget(Qt::LeftDockWidgetArea, m_projectDock);

    m_propertyDock = new QDockWidget(QStringLiteral("属性"), this);
    m_propertyDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_propertyDock->setMinimumHeight(220);

    m_propertyTable = new QTableWidget(7, 2, m_propertyDock);
    m_propertyTable->setHorizontalHeaderLabels({QStringLiteral("属性"), QStringLiteral("值")});
    m_propertyTable->horizontalHeader()->setStretchLastSection(true);
    m_propertyTable->verticalHeader()->setVisible(false);
    m_propertyTable->setAlternatingRowColors(true);
    m_propertyTable->setShowGrid(false);
    m_propertyTable->setItem(0, 0, new QTableWidgetItem(QStringLiteral("背景色")));
    m_propertyTable->setItem(0, 1, new QTableWidgetItem(QStringLiteral("255,255,255")));
    m_propertyTable->setItem(1, 0, new QTableWidgetItem(QStringLiteral("网格")));
    m_propertyTable->setItem(1, 1, new QTableWidgetItem(QStringLiteral("20")));
    m_propertyTable->setItem(2, 0, new QTableWidgetItem(QStringLiteral("场景宽")));
    m_propertyTable->setItem(2, 1, new QTableWidgetItem(QStringLiteral("3600")));
    m_propertyTable->setItem(3, 0, new QTableWidgetItem(QStringLiteral("场景高")));
    m_propertyTable->setItem(3, 1, new QTableWidgetItem(QStringLiteral("2400")));
    m_propertyTable->setItem(4, 0, new QTableWidgetItem(QStringLiteral("吸附")));
    m_propertyTable->setItem(4, 1, new QTableWidgetItem(QStringLiteral("开启")));
    m_propertyTable->setItem(5, 0, new QTableWidgetItem(QStringLiteral("缩放")));
    m_propertyTable->setItem(5, 1, new QTableWidgetItem(QStringLiteral("100%")));
    m_propertyTable->setItem(6, 0, new QTableWidgetItem(QStringLiteral("版本")));
    m_propertyTable->setItem(6, 1, new QTableWidgetItem(QStringLiteral("UI-Proto-0.1")));
    m_propertyDock->setWidget(m_propertyTable);
    addDockWidget(Qt::LeftDockWidgetArea, m_propertyDock);

    splitDockWidget(m_projectDock, m_propertyDock, Qt::Vertical);

    m_viewMenu->addAction(m_projectDock->toggleViewAction());
    m_viewMenu->addAction(m_propertyDock->toggleViewAction());
}

void MainWindow::setupRightDock() {
    m_paletteDock = new QDockWidget(QStringLiteral("工具箱"), this);
    m_paletteDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_paletteDock->setMinimumWidth(260);

    QWidget* panel = new QWidget(m_paletteDock);
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    QToolBox* toolBox = new QToolBox(panel);
    auto addCategoryPage = [this, toolBox](const QString& title, const QStringList& names) {
        auto* list = new QListWidget(toolBox);
        list->setViewMode(QListView::IconMode);
        list->setResizeMode(QListView::Adjust);
        list->setIconSize(QSize(36, 36));
        list->setSpacing(8);
        list->setMovement(QListView::Static);
        for (const QString& itemName : names) {
            auto* item = new QListWidgetItem(style()->standardIcon(QStyle::SP_FileDialogContentsView), itemName);
            list->addItem(item);
        }
        toolBox->addItem(list, title);
    };

    addCategoryPage(QStringLiteral("Actuator"), {QStringLiteral("tm_CheckVlv"), QStringLiteral("tm_AirWater"), QStringLiteral("tm_Bound"), QStringLiteral("tm_Node")});
    addCategoryPage(QStringLiteral("Sensor"), {QStringLiteral("tm_TubeHte"), QStringLiteral("tm_HeatExChS"), QStringLiteral("tm_HeatExCh"), QStringLiteral("tm_Vessel")});
    addCategoryPage(QStringLiteral("Control"), {QStringLiteral("tm_2PhaseVessel"), QStringLiteral("tm_Accumulator"), QStringLiteral("tm_SteamTrap"), QStringLiteral("tm_Bearing")});
    addCategoryPage(QStringLiteral("Electric"), {QStringLiteral("tm_Load"), QStringLiteral("tm_HeatSide"), QStringLiteral("tm_Valve"), QStringLiteral("tm_Pump")});

    layout->addWidget(toolBox);
    m_paletteDock->setWidget(panel);
    addDockWidget(Qt::RightDockWidgetArea, m_paletteDock);

    m_viewMenu->addAction(m_paletteDock->toggleViewAction());
}

void MainWindow::populateDemoGraph() {
    auto addLabel = [this](const QString& text, const QPointF& pos) {
        auto* label = m_scene->addSimpleText(text, QFont(QStringLiteral("Segoe UI"), 9));
        label->setBrush(QColor(68, 68, 68));
        label->setPos(pos);
    };

    auto addNode = [this](const QString& name, const QPointF& pos, const QSizeF& size = QSizeF(92, 30)) {
        auto* node = new QGraphicsRectItem(0, 0, size.width(), size.height());
        node->setPos(pos);
        node->setPen(QPen(QColor(141, 154, 183), 1.2));
        node->setBrush(QColor(224, 233, 247));
        node->setFlag(QGraphicsItem::ItemIsMovable, true);
        node->setFlag(QGraphicsItem::ItemIsSelectable, true);

        auto* text = new QGraphicsSimpleTextItem(name, node);
        text->setPos(6, 8);
        text->setBrush(QColor(39, 67, 197));
        m_scene->addItem(node);
        return node;
    };

    auto addEdge = [this](QGraphicsRectItem* src, QGraphicsRectItem* dst) {
        const QPointF start = src->scenePos() + QPointF(src->rect().width(), src->rect().height() * 0.5);
        const QPointF end = dst->scenePos() + QPointF(0, dst->rect().height() * 0.5);
        const qreal midX = (start.x() + end.x()) * 0.5;
        QPainterPath path(start);
        path.lineTo(midX, start.y());
        path.lineTo(midX, end.y());
        path.lineTo(end);
        auto* edge = new QGraphicsPathItem(path);
        edge->setPen(QPen(QColor(85, 85, 85), 1.4));
        edge->setZValue(-1);
        m_scene->addItem(edge);
    };

    addLabel(QStringLiteral("一环路热负荷变量"), QPointF(35, 120));
    addLabel(QStringLiteral("一环路热负荷平衡量平均值"), QPointF(35, 320));
    addLabel(QStringLiteral("二环路热负荷变量"), QPointF(35, 540));

    auto* n1 = addNode(QStringLiteral("RCP1833MT"), QPointF(300, 130));
    auto* n2 = addNode(QStringLiteral("RCP1832MT"), QPointF(300, 205));
    auto* n3 = addNode(QStringLiteral("RCP1834MT"), QPointF(300, 280));
    auto* n4 = addNode(QStringLiteral("RCP1835MT"), QPointF(300, 355));
    auto* v1 = addNode(QStringLiteral("Voter4"), QPointF(500, 220), QSizeF(82, 120));
    auto* s1 = addNode(QStringLiteral("SFT"), QPointF(680, 225), QSizeF(74, 52));
    auto* a1 = addNode(QStringLiteral("RCP_SH1804_01"), QPointF(850, 150));
    auto* sum1 = addNode(QStringLiteral("Sum"), QPointF(1020, 224), QSizeF(62, 36));
    auto* v2 = addNode(QStringLiteral("Voter3"), QPointF(1280, 195), QSizeF(82, 95));
    auto* o1 = addNode(QStringLiteral("RCP0001KM"), QPointF(1460, 196));

    auto* n5 = addNode(QStringLiteral("RCP2833MT"), QPointF(300, 545));
    auto* n6 = addNode(QStringLiteral("RCP2832MT"), QPointF(300, 620));
    auto* n7 = addNode(QStringLiteral("RCP2834MT"), QPointF(300, 695));
    auto* n8 = addNode(QStringLiteral("RCP2835MT"), QPointF(300, 770));
    auto* v3 = addNode(QStringLiteral("Voter4"), QPointF(500, 608), QSizeF(82, 120));
    auto* s2 = addNode(QStringLiteral("SFT"), QPointF(680, 640), QSizeF(74, 52));
    auto* a2 = addNode(QStringLiteral("RCP_SH2804_01"), QPointF(850, 570));
    auto* sum2 = addNode(QStringLiteral("Sum"), QPointF(1020, 640), QSizeF(62, 36));

    addEdge(n1, v1);
    addEdge(n2, v1);
    addEdge(n3, v1);
    addEdge(n4, v1);
    addEdge(v1, s1);
    addEdge(s1, a1);
    addEdge(a1, sum1);
    addEdge(sum1, v2);
    addEdge(v2, o1);

    addEdge(n5, v3);
    addEdge(n6, v3);
    addEdge(n7, v3);
    addEdge(n8, v3);
    addEdge(v3, s2);
    addEdge(s2, a2);
    addEdge(a2, sum2);
    addEdge(sum2, v2);
}
