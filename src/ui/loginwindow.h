#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

/**
 * @file loginwindow.h
 * @brief Declarative definition of the modal authentication dialog.
 *
 * Presents email/password fields, validates user input, and delegates authentication
 * to the Supabase client. On success, the dialog stores credentials and returns an
 * accepted result to the caller.
 *
 * @example
 * LoginWindow login;
 * if (login.exec() == QDialog::Accepted) {
 *     LoginSession s = login.session();
 *     // Use s.email, s.password, s.auth.token...
 * }
 */

#include "../core/SupabaseClient.h"
#include "windowEdit/FramelessWindow.h"
#include <QDialog>

class QLineEdit;
class QPushButton;
class QLabel;

/**
 * @brief Modal dialog responsible for authenticating the user against Supabase.
 *
 * Renders email/password inputs, displays status feedback, and exposes the resulting
 * `LoginSession` for downstream initialization. The dialog remains frameless to match
 * the app chrome styling.
 *
 * @example
 * LoginWindow dlg;
 * dlg.exec();
 * auto session = dlg.session();
 */
class LoginWindow : public FramelessDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the dialog, wires Supabase callbacks, and builds the UI.
     *
     * Sets modal behavior and prevents deletion on close so session data remains
     * available to callers after acceptance.
     *
     * @param parent Optional parent widget for modality scoping.
     * @throws std::bad_alloc If widget creation fails.
     * @example LoginWindow login(nullptr);
     */
    explicit LoginWindow(QWidget* parent = nullptr);
    /**
     * @brief Returns the most recent authenticated session payload.
     * @return LoginSession containing email/password and bearer token details.
     * @throws None
     * @example LoginSession s = login.session();
     */
    LoginSession session() const { return currentSession; }

private slots:
    /**
     * @brief Validates inputs and initiates Supabase login.
     *
     * Trims whitespace from the email, guards against concurrent logins, and updates
     * UI state to indicate busy progress.
     *
     * @return void
     * @throws None
     * @example attemptLogin();
     */
    void attemptLogin();
    /**
     * @brief Consumes Supabase login results, updates session data, and closes on success.
     *
     * Shows inline error feedback when authentication fails.
     *
     * @param result Authentication result from Supabase.
     * @return void
     * @throws None
     * @example handleLoginResult(result);
     */
    void handleLoginResult(const AuthResult& result);

private:
    /**
     * @brief Assembles form controls and applies basic styling.
     * @return void
     * @throws std::bad_alloc If layout/widget creation fails.
     * @example buildUi();
     */
    void buildUi();
    /**
     * @brief Toggles UI interactivity during network calls and shows an optional message.
     *
     * Disables inputs and button while login is in progress.
     *
     * @param busy True to disable controls and show progress text.
     * @param message Optional status text to display.
     * @return void
     * @throws None
     * @example setBusy(true, tr("Signing in..."));
     */
    void setBusy(bool busy, const QString& message = QString());
    /**
     * @brief Updates the status label with success or error styling.
     *
     * Clears the label when empty text is provided.
     *
     * @param text Message to display.
     * @param isError True to render in error color.
     * @return void
     * @throws None
     * @example updateStatus(tr("Invalid password"), true);
     */
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
