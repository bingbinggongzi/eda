#include "MainWindow.h"

#include <QApplication>
#include <QFont>
#include <QPalette>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    app.setStyle(QStringLiteral("Fusion"));
    app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));

    QPalette palette = app.palette();
    palette.setColor(QPalette::Window, QColor(236, 236, 236));
    palette.setColor(QPalette::Base, QColor(250, 250, 250));
    palette.setColor(QPalette::AlternateBase, QColor(244, 244, 244));
    palette.setColor(QPalette::Button, QColor(235, 235, 235));
    app.setPalette(palette);

    MainWindow mainWindow;
    mainWindow.show();
    return app.exec();
}
