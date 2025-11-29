#include "ui/loginwindow.h"
#include "ui/mainwindow.h"
#include "core/AppLogger.h"
#include <QApplication>
#include <QDialog>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    AppLogger::initialize();

    LoginWindow login;
    if (login.exec() != QDialog::Accepted)
        return 0;

    MainWindow w;
    w.initializeServerSync(login.session());
    w.show();
    return a.exec();
}
