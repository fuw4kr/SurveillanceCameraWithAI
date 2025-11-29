#ifndef UNKNOWNFACEDIALOG_H
#define UNKNOWNFACEDIALOG_H

#include <QDialog>
#include <QImage>

class QLabel;
class QLineEdit;
class QCheckBox;
class QPushButton;
class QLabel;

class UnknownFaceDialog : public QDialog
{
    Q_OBJECT

public:
    UnknownFaceDialog(const QImage& snapshot, const QString& cameraLabel, qreal confidence, QWidget* parent = nullptr);

    void setBusyState(const QString& text);
    void showError(const QString& text);
    void showSuccess(const QString& text);

signals:
    void markUnknown();
    void savePerson(const QString& name, const QString& role, bool authorized);
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
