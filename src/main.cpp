#include "MainWindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/assets/app_icon.xpm"));
    MainWindow w;
    w.show();
    return app.exec();
}
