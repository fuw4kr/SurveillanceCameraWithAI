#include "UnknownFaceDialog.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>

UnknownFaceDialog::UnknownFaceDialog(const QImage& snapshot, const QString& cameraLabel, qreal confidence, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Unknown face detected"));
    setModal(true);
    setAttribute(Qt::WA_DeleteOnClose);
    setupUi(snapshot, cameraLabel, confidence);
}

void UnknownFaceDialog::setupUi(const QImage& snapshot, const QString& cameraLabel, qreal confidence)
{
    auto* mainLayout = new QVBoxLayout(this);
    auto* title = new QLabel(tr("Підтвердіть дію з розпізнаним обличчям"));
    title->setStyleSheet(QStringLiteral("font-size:16px; font-weight:600;"));
    mainLayout->addWidget(title);

    auto* cameraLabelWidget = new QLabel(
        tr("Камера: <b>%1</b><br/>Достовірність: <b>%2%</b>")
            .arg(cameraLabel,
                QString::number(static_cast<int>(confidence * 100.0))));
    cameraLabelWidget->setTextFormat(Qt::RichText);
    mainLayout->addWidget(cameraLabelWidget);

    previewLabel = new QLabel(this);
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setFixedSize(260, 260);
    previewLabel->setStyleSheet(QStringLiteral("background:#111827; border:1px solid #1f2937; border-radius:12px;"));
    if (!snapshot.isNull())
        previewLabel->setPixmap(QPixmap::fromImage(snapshot).scaled(previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    mainLayout->addWidget(previewLabel, 0, Qt::AlignHCenter);

    auto* formLayout = new QVBoxLayout;
    auto* nameLabel = new QLabel(tr("Ім'я співробітника (якщо відоме):"));
    nameEdit = new QLineEdit(this);
    nameEdit->setPlaceholderText(tr("Ім'я та прізвище"));
    auto* roleLabel = new QLabel(tr("Роль / посада:"));
    roleEdit = new QLineEdit(this);
    roleEdit->setPlaceholderText(tr("Напр. Охоронець, Гість, Співробітник"));
    authorizedCheck = new QCheckBox(tr("Дозволений доступ"), this);

    formLayout->addWidget(nameLabel);
    formLayout->addWidget(nameEdit);
    formLayout->addWidget(roleLabel);
    formLayout->addWidget(roleEdit);
    formLayout->addWidget(authorizedCheck);
    mainLayout->addLayout(formLayout);

    auto* buttonLayout = new QHBoxLayout;
    unknownButton = new QPushButton(tr("Невідоме обличчя"), this);
    unknownButton->setStyleSheet(QStringLiteral("background:#f97316; color:white; font-weight:600;"));
    saveButton = new QPushButton(tr("Зберегти як відоме"), this);
    saveButton->setStyleSheet(QStringLiteral("background:#16a34a; color:white; font-weight:600;"));
    skipButton = new QPushButton(tr("Пропустити"), this);

    buttonLayout->addWidget(unknownButton);
    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(skipButton);
    mainLayout->addLayout(buttonLayout);

    statusInfo = new QLabel(this);
    statusInfo->setStyleSheet(QStringLiteral("color:#94a3b8; font-size:12px;"));
    statusInfo->setWordWrap(true);
    mainLayout->addWidget(statusInfo);

    connect(unknownButton, &QPushButton::clicked, this, [this]() {
        emit markUnknown();
        close();
    });
    connect(saveButton, &QPushButton::clicked, this, [this]() {
        if (busy)
            return;
        const QString name = nameEdit->text().trimmed();
        const QString role = roleEdit->text().trimmed();
        if (name.isEmpty() || role.isEmpty()) {
            QMessageBox::warning(this, tr("Заповніть поля"), tr("Вкажіть ім'я та роль перед збереженням."));
            return;
        }
        emit savePerson(name, role, authorizedCheck->isChecked());
        setBusyState(tr("Створюємо запис на сервері..."));
    });
    connect(skipButton, &QPushButton::clicked, this, [this]() {
        emit skipped();
        close();
    });
}

void UnknownFaceDialog::setButtonsEnabled(bool enabled)
{
    if (unknownButton)
        unknownButton->setEnabled(enabled);
    if (saveButton)
        saveButton->setEnabled(enabled);
    if (skipButton)
        skipButton->setEnabled(enabled);
}

void UnknownFaceDialog::setBusyState(const QString& text)
{
    busy = true;
    setButtonsEnabled(false);
    if (statusInfo) {
        statusInfo->setStyleSheet(QStringLiteral("color:#60a5fa; font-size:12px;"));
        statusInfo->setText(text);
    }
}

void UnknownFaceDialog::showError(const QString& text)
{
    busy = false;
    setButtonsEnabled(true);
    if (statusInfo) {
        statusInfo->setStyleSheet(QStringLiteral("color:#f87171; font-size:12px;"));
        statusInfo->setText(text);
    }
}

void UnknownFaceDialog::showSuccess(const QString& text)
{
    if (statusInfo) {
        statusInfo->setStyleSheet(QStringLiteral("color:#a7f3d0; font-size:12px;"));
        statusInfo->setText(text);
    }
}
