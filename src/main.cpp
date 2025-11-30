/**
 * @file main.cpp
 * @brief Entry point that initializes Qt, configures logging, and starts the main UI flow.
 *
 * The application boots Qt, sets up the logging backend, enforces an authentication
 * gate through the modal login dialog, and then launches the primary MainWindow with
 * an already-authenticated session. This centralizes boot-time concerns so that the
 * rest of the UI stack can assume a valid session and initialized logging facilities.
 *
 * @example
 * // Launch the binary with an optional Qt style override; the login dialog appears first.
 * // > ./AppExecutable --style fusion
 */
#include "ui/loginwindow.h"
#include "ui/mainwindow.h"
#include "core/AppLogger.h"
#include <QApplication>
#include <QDialog>
#include <QIcon>

/**
 * @brief Launches the application, enforcing authentication before showing the main window.
 *
 * The function configures Qt, initializes the shared logger, prompts the user to authenticate,
 * and, upon success, wires the authenticated session into the main UI before entering the
 * event loop.
 *
 * @param argc Count of command-line arguments provided by the host process.
 * @param argv Command-line argument array forwarded to Qt for built-in flags (e.g., styling, DPI).
 * @return Event loop exit code from `QApplication::exec()`, or `0` if the login dialog is rejected.
 * @throws std::exception If Qt fails to initialize or if `AppLogger::initialize()` propagates an error.
 * @example
 * // Launch with a custom Qt style while relying on the built-in login flow.
 * // The application exits early if authentication fails.
 * int status = main(argc, argv);
 * if (status != 0) {
 *     // Optional: log or react to non-zero exit codes.
 * }
 */
int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(QStringLiteral(":/resources/icons/appicon.png")));
    AppLogger::initialize();

    LoginWindow login;
    if (login.exec() != QDialog::Accepted)
        return 0;

    MainWindow w;
    w.initializeServerSync(login.session());
    w.show();
    return a.exec();
}
