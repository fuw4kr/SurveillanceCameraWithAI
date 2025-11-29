#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include "../core/SupabaseClient.h"
#include "windowEdit/FramelessWindow.h"
#include <QDialog>

class QLineEdit;
class QPushButton;
class QLabel;

class LoginWindow : public FramelessDialog
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget* parent = nullptr);
    LoginSession session() const { return currentSession; }

private slots:
    void attemptLogin();
    void handleLoginResult(const AuthResult& result);

private:
    void buildUi();
    void setBusy(bool busy, const QString& message = QString());
    void updateStatus(const QString& text, bool isError = false);

    SupabaseClient* client = nullptr;
    QLineEdit* emailEdit = nullptr;
    QLineEdit* passwordEdit = nullptr;
    QPushButton* loginButton = nullptr;
    QLabel* statusLabel = nullptr;
    QLabel* titleLabel = nullptr;
    LoginSession currentSession;
    QString pendingEmail;
    QString pendingPassword;
    bool busy = false;
};

#endif // LOGINWINDOW_H
