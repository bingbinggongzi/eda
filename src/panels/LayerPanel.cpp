#include "LayerPanel.h"

#include "scene/EditorScene.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

LayerPanel::LayerPanel(QWidget* parent)
    : QWidget(parent) {
    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("A"), QStringLiteral("V"), QStringLiteral("L"), QStringLiteral("Name"), QStringLiteral("Nodes")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    m_addButton = new QPushButton(QStringLiteral("+"), this);
    m_deleteButton = new QPushButton(QStringLiteral("-"), this);
    m_upButton = new QPushButton(QStringLiteral("Up"), this);
    m_downButton = new QPushButton(QStringLiteral("Down"), this);
    m_activeButton = new QPushButton(QStringLiteral("Set Active"), this);
    m_moveSelectionButton = new QPushButton(QStringLiteral("Move Selection"), this);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(6);
    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_deleteButton);
    buttonLayout->addWidget(m_upButton);
    buttonLayout->addWidget(m_downButton);
    buttonLayout->addWidget(m_activeButton);
    buttonLayout->addWidget(m_moveSelectionButton);
    buttonLayout->addStretch(1);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addLayout(buttonLayout);
    layout->addWidget(m_table);

    connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (m_updating || !item || !m_scene) {
            return;
        }
        const QString layerId = layerIdAtRow(item->row());
        if (layerId.isEmpty()) {
            return;
        }
        bool changed = false;
        if (item->column() == 1) {
            changed = m_scene->setLayerVisibleWithUndo(layerId, item->checkState() == Qt::Checked);
        } else if (item->column() == 2) {
            changed = m_scene->setLayerLockedWithUndo(layerId, item->checkState() == Qt::Checked);
        } else if (item->column() == 3) {
            changed = m_scene->renameLayerWithUndo(layerId, item->text());
        }
        if (changed) {
            refresh();
        }
    });

    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
        if (!m_scene || column != 0) {
            return;
        }
        const QString layerId = layerIdAtRow(row);
        if (!layerId.isEmpty() && m_scene->setActiveLayerWithUndo(layerId)) {
            refresh();
        }
    });

    connect(m_addButton, &QPushButton::clicked, this, [this]() {
        if (!m_scene) {
            return;
        }
        bool ok = false;
        const QString name = QInputDialog::getText(
            this, QStringLiteral("Add Layer"), QStringLiteral("Layer name:"), QLineEdit::Normal, QString(), &ok);
        if (!ok) {
            return;
        }
        m_scene->createLayerWithUndo(name);
        refresh();
    });

    connect(m_deleteButton, &QPushButton::clicked, this, [this]() {
        if (!m_scene) {
            return;
        }
        const int row = currentLayerRow();
        const QString layerId = layerIdAtRow(row);
        if (!layerId.isEmpty() && m_scene->deleteLayerWithUndo(layerId)) {
            refresh();
        }
    });

    connect(m_upButton, &QPushButton::clicked, this, [this]() {
        if (!m_scene) {
            return;
        }
        const int row = currentLayerRow();
        const QString layerId = layerIdAtRow(row);
        if (!layerId.isEmpty() && m_scene->moveLayerWithUndo(layerId, row - 1)) {
            refresh();
        }
    });

    connect(m_downButton, &QPushButton::clicked, this, [this]() {
        if (!m_scene) {
            return;
        }
        const int row = currentLayerRow();
        const QString layerId = layerIdAtRow(row);
        if (!layerId.isEmpty() && m_scene->moveLayerWithUndo(layerId, row + 1)) {
            refresh();
        }
    });

    connect(m_activeButton, &QPushButton::clicked, this, [this]() {
        if (!m_scene) {
            return;
        }
        const int row = currentLayerRow();
        const QString layerId = layerIdAtRow(row);
        if (!layerId.isEmpty() && m_scene->setActiveLayerWithUndo(layerId)) {
            refresh();
        }
    });

    connect(m_moveSelectionButton, &QPushButton::clicked, this, [this]() {
        if (!m_scene) {
            return;
        }
        const int row = currentLayerRow();
        const QString layerId = layerIdAtRow(row);
        if (!layerId.isEmpty() && m_scene->moveSelectionToLayerWithUndo(layerId)) {
            refresh();
        }
    });
}

void LayerPanel::setScene(EditorScene* scene) {
    if (m_scene == scene) {
        refresh();
        return;
    }
    if (m_scene) {
        disconnect(m_scene, nullptr, this, nullptr);
    }
    m_scene = scene;
    if (m_scene) {
        connect(m_scene, &EditorScene::graphChanged, this, &LayerPanel::refresh);
        connect(m_scene, &EditorScene::layerStateChanged, this, &LayerPanel::refresh);
    }
    refresh();
}

void LayerPanel::refresh() {
    m_updating = true;
    const QSignalBlocker blocker(m_table);

    QString selectedLayerId;
    const int currentRow = currentLayerRow();
    if (currentRow >= 0) {
        selectedLayerId = layerIdAtRow(currentRow);
    }

    m_table->clearContents();
    m_table->setRowCount(0);

    if (!m_scene) {
        m_updating = false;
        return;
    }

    const QVector<LayerData> layerList = m_scene->layers();
    m_table->setRowCount(layerList.size());
    const QString activeId = m_scene->activeLayerId();
    int activeRow = -1;
    int restoreRow = -1;
    for (int row = 0; row < layerList.size(); ++row) {
        const LayerData& layer = layerList[row];

        auto* activeItem = new QTableWidgetItem(layer.id == activeId ? QStringLiteral("*") : QString());
        activeItem->setFlags(activeItem->flags() & ~Qt::ItemIsEditable);
        activeItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 0, activeItem);

        auto* visibleItem = new QTableWidgetItem();
        visibleItem->setFlags((visibleItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        visibleItem->setCheckState(layer.visible ? Qt::Checked : Qt::Unchecked);
        m_table->setItem(row, 1, visibleItem);

        auto* lockedItem = new QTableWidgetItem();
        lockedItem->setFlags((lockedItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        lockedItem->setCheckState(layer.locked ? Qt::Checked : Qt::Unchecked);
        m_table->setItem(row, 2, lockedItem);

        auto* nameItem = new QTableWidgetItem(layer.name);
        nameItem->setData(Qt::UserRole, layer.id);
        m_table->setItem(row, 3, nameItem);

        auto* countItem = new QTableWidgetItem(QString::number(m_scene->layerNodeCount(layer.id)));
        countItem->setFlags(countItem->flags() & ~Qt::ItemIsEditable);
        countItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 4, countItem);

        if (layer.id == activeId) {
            activeRow = row;
        }
        if (!selectedLayerId.isEmpty() && selectedLayerId == layer.id) {
            restoreRow = row;
        }
    }

    const int targetRow = (restoreRow >= 0) ? restoreRow : activeRow;
    if (targetRow >= 0) {
        m_table->selectRow(targetRow);
    }
    m_updating = false;
}

QString LayerPanel::layerIdAtRow(int row) const {
    if (row < 0 || row >= m_table->rowCount()) {
        return QString();
    }
    QTableWidgetItem* nameItem = m_table->item(row, 3);
    return nameItem ? nameItem->data(Qt::UserRole).toString() : QString();
}

int LayerPanel::currentLayerRow() const {
    if (!m_table) {
        return -1;
    }
    const QModelIndexList selectedRows = m_table->selectionModel() ? m_table->selectionModel()->selectedRows() : QModelIndexList();
    if (!selectedRows.isEmpty()) {
        return selectedRows.first().row();
    }
    return m_table->currentRow();
}
