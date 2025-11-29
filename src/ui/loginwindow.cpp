/**
 * @file loginwindow.cpp
 * @brief Implementation of the modal authentication dialog backed by Supabase.
 *
 * Builds the email/password form, sanitizes user input, and forwards credentials to
 * the Supabase client while providing inline status feedback.
 *
 * @example
 * LoginWindow login;
 * if (login.exec() == QDialog::Accepted) {
 *     auto session = login.session();
 * }
 */
#include "loginwindow.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
/**
 * @brief Normalizes the supplied email by trimming whitespace.
 * @param email Raw user-entered email value.
 * @return QString Trimmed email suitable for authentication.
 * @throws None
 * @example QString normalized = sanitizeEmail(" user@example.com ");
 */
QString sanitizeEmail(const QString& email)
{
    return email.trimmed();
}
}

LoginWindow::LoginWindow(QWidget* parent)
    : FramelessDialog(parent)
{
    setModal(true);
    setWindowTitle(tr("Sign in"));
    setAttribute(Qt::WA_DeleteOnClose, false);
    resize(420, 360);

    client = new SupabaseClient(this);
    connect(client, &SupabaseClient::loginFinished, this, &LoginWindow::handleLoginResult);

    buildUi();
}

void LoginWindow::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(16);

    titleLabel = new QLabel(tr("AI Surveillance Portal"), this);
    titleLabel->setStyleSheet("font-size:22px; font-weight:600; color:#f8fafc;");
    QLabel* subtitle = new QLabel(tr("Enter your credentials to access the dashboard"), this);
    subtitle->setWordWrap(true);
    subtitle->setStyleSheet("color:#94a3b8;");

    layout->addWidget(titleLabel);
    layout->addWidget(subtitle);

    emailEdit = new QLineEdit(this);
    emailEdit->setPlaceholderText(tr("Email address"));
    emailEdit->setClearButtonEnabled(true);
    emailEdit->setMinimumHeight(38);

    passwordEdit = new QLineEdit(this);
    passwordEdit->setPlaceholderText(tr("Password"));
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setClearButtonEnabled(true);
    passwordEdit->setMinimumHeight(38);

    loginButton = new QPushButton(tr("Sign in"), this);
    loginButton->setMinimumHeight(40);
    loginButton->setStyleSheet("QPushButton { background:#2563eb; color:white; border:none; border-radius:8px; font-weight:600; }"
                               "QPushButton:disabled { background:#1f2937; color:#94a3b8; }");

    statusLabel = new QLabel(this);
    statusLabel->setStyleSheet("color:#a7f3d0;");
    statusLabel->setWordWrap(true);

    layout->addWidget(emailEdit);
    layout->addWidget(passwordEdit);
    layout->addSpacing(8);
    layout->addWidget(loginButton);
    layout->addWidget(statusLabel);
    layout->addStretch();

    connect(loginButton, &QPushButton::clicked, this, &LoginWindow::attemptLogin);
    connect(passwordEdit, &QLineEdit::returnPressed, this, &LoginWindow::attemptLogin);
    connect(emailEdit, &QLineEdit::returnPressed, this, &LoginWindow::attemptLogin);
}

void LoginWindow::setBusy(bool isBusy, const QString& message)
{
    busy = isBusy;
    emailEdit->setEnabled(!isBusy);
    passwordEdit->setEnabled(!isBusy);
    loginButton->setEnabled(!isBusy);
    if (!message.isEmpty())
        updateStatus(message, false);
}

void LoginWindow::updateStatus(const QString& text, bool isError)
{
    if (!statusLabel)
        return;
    if (text.isEmpty()) {
        statusLabel->clear();
        return;
    }
    statusLabel->setStyleSheet(isError ? "color:#f87171;" : "color:#a7f3d0;");
    statusLabel->setText(text);
}

void LoginWindow::attemptLogin()
{
    if (busy)
        return;
    const QString email = sanitizeEmail(emailEdit->text());
    const QString password = passwordEdit->text();
    if (email.isEmpty() || password.isEmpty()) {
        updateStatus(tr("Enter email and password"), true);
        return;
    }

    pendingEmail = email;
    pendingPassword = password;
    qInfo() << "[Login]" << "Attempting authentication for" << email;
    setBusy(true, tr("Signing in..."));
    client->login(email, password);
}

void LoginWindow::handleLoginResult(const AuthResult& result)
{
    setBusy(false);
    if (!result.success) {
        qWarning() << "[Login]" << "Authentication failed:" << result.message;
        updateStatus(result.message.isEmpty() ? tr("Login failed. Check credentials.") : result.message, true);
        return;
    }

    currentSession.email = pendingEmail;
    currentSession.password = pendingPassword;
    currentSession.auth = result;
    updateStatus(tr("Welcome back!"), false);
    qInfo() << "[Login]" << "Authentication succeeded. Token expires at" << result.expiresAt.toString(Qt::ISODate);
    accept();
}
