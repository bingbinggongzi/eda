#include "PropertyPanel.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QTableWidget>
#include <QVBoxLayout>

PropertyPanel::PropertyPanel(QWidget* parent)
    : QWidget(parent) {
    m_table = new QTableWidget(9, 2, this);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Key"), QStringLiteral("Value")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_table);
}

QTableWidget* PropertyPanel::table() const {
    return m_table;
}
