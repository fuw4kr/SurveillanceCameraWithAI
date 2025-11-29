#ifndef UNKNOWNFACEDIALOG_H
#define UNKNOWNFACEDIALOG_H

/**
 * @file UnknownFaceDialog.h
 * @brief Modal dialog prompting users to classify an unknown face detection.
 *
 * Displays a snapshot, camera label, and confidence score, allowing operators to
 * mark the face as unknown or save it as a known person with role/authorization.
 *
 * @example
 * auto* dlg = new UnknownFaceDialog(image, "Cam 1", 0.92, parent);
 * connect(dlg, &UnknownFaceDialog::savePerson, ...);
 * dlg->open();
 */

#include <QDialog>
#include <QImage>

class QLabel;
class QLineEdit;
class QCheckBox;
class QPushButton;
class QLabel;

/**
 * @brief Dialog for handling unknown face alerts with options to enroll or dismiss.
 */
class UnknownFaceDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Builds the dialog UI with snapshot and metadata.
     * @param snapshot Cropped face image.
     * @param cameraLabel Human-readable camera identifier.
     * @param confidence Detection confidence.
     * @param parent Optional parent widget.
     */
    UnknownFaceDialog(const QImage& snapshot, const QString& cameraLabel, qreal confidence, QWidget* parent = nullptr);

    /**
     * @brief Shows a busy message and disables buttons during server operations.
     * @param text Status text to display.
     */
    void setBusyState(const QString& text);
    /**
     * @brief Displays an error message and re-enables controls.
     * @param text Error description.
     */
    void showError(const QString& text);
    /**
     * @brief Displays a success message and keeps controls disabled.
     * @param text Success description.
     */
    void showSuccess(const QString& text);

signals:
    /**
     * @brief Emitted when the user marks the face as unknown.
     */
    void markUnknown();
    /**
     * @brief Emitted when the user saves the face as a known person.
     * @param name Person name.
     * @param role Role/department.
     * @param authorized Authorization flag.
     */
    void savePerson(const QString& name, const QString& role, bool authorized);
    /**
     * @brief Emitted when the dialog is skipped without action.
     */
    void skipped();

private:
    void setupUi(const QImage& snapshot, const QString& cameraLabel, qreal confidence);

    QLabel* previewLabel = nullptr;
    QLineEdit* nameEdit = nullptr;
    QLineEdit* roleEdit = nullptr;
    QCheckBox* authorizedCheck = nullptr;
    QPushButton* unknownButton = nullptr;
    QPushButton* saveButton = nullptr;
    QPushButton* skipButton = nullptr;
    QLabel* statusInfo = nullptr;
    bool busy = false;

    void setButtonsEnabled(bool enabled);
};

#endif // UNKNOWNFACEDIALOG_H
