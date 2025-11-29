#ifndef FACEDATABASEPAGE_H
#define FACEDATABASEPAGE_H

#include <QPixmap>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QPushButton;
class QLabel;
class ServerSyncManager;

#include "../../core/ServerTypes.h"

class FaceDatabasePage : public QWidget
{
    Q_OBJECT
public:
    explicit FaceDatabasePage(ServerSyncManager* sync, QWidget* parent = nullptr);
    void setRemotePersons(const QList<PersonRecord>& persons);

signals:
    void requestCloudRefresh();

private slots:
    void handleSearchChanged(const QString& text);
    void handleSelectionChanged();
    void handleRename();
    void handleDelete();
    void handleNameEdited(const QString& text);
    void handleRefreshClicked();

private:
    ServerSyncManager* serverSync = nullptr;
    QLineEdit* searchInput = nullptr;
    QListWidget* gallery = nullptr;
    QLabel* previewLabel = nullptr;
    QLabel* samplesLabel = nullptr;
    QLabel* infoLabel = nullptr;
    QLabel* statusLabel = nullptr;
    QLineEdit* nameEdit = nullptr;
    QPushButton* renameButton = nullptr;
    QPushButton* deleteButton = nullptr;
    QPushButton* refreshButton = nullptr;

    QList<PersonRecord> remotePersons;
    QString currentFilter;
    bool updatingSelection = false;

    void rebuildGallery();
    QStringList selectedIds() const;
    QString selectedPrimaryId() const;
    PersonRecord personById(const QString& id) const;
    void updateDetailPanel();
    QPixmap buildFacePixmap(const PersonRecord& person, const QSize& size) const;
    void setStatusMessage(const QString& text, bool isError = false);
};

#endif // FACEDATABASEPAGE_H
