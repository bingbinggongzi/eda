#pragma once

#include <QWidget>

class QTableWidget;

class PropertyPanel : public QWidget {
    Q_OBJECT

public:
    explicit PropertyPanel(QWidget* parent = nullptr);

    QTableWidget* table() const;

private:
    QTableWidget* m_table = nullptr;
};
