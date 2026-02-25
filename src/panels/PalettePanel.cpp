#include "PalettePanel.h"

#include "model/ComponentCatalog.h"

#include <QAbstractItemView>
#include <QListWidget>
#include <QListView>
#include <QStyle>
#include <QToolBox>
#include <QVBoxLayout>

namespace {
void addPaletteCategory(QToolBox* toolBox, QWidget* owner, const QString& title, const QStringList& names) {
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
        auto* item = new QListWidgetItem(owner->style()->standardIcon(QStyle::SP_FileDialogContentsView), itemName);
        item->setData(Qt::UserRole, itemName);
        list->addItem(item);
    }
    toolBox->addItem(list, title);
}
}  // namespace

PalettePanel::PalettePanel(QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    QToolBox* toolBox = new QToolBox(this);
    const ComponentCatalog& catalog = ComponentCatalog::instance();
    const QStringList categories = catalog.categories();
    for (const QString& category : categories) {
        addPaletteCategory(toolBox, this, category, catalog.typesInCategory(category));
    }
    layout->addWidget(toolBox);
}
