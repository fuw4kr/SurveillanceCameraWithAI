#ifndef FACEDATABASEPAGE_H
#define FACEDATABASEPAGE_H

/**
 * @file faceDatabasePage.h
 * @brief UI page for browsing and managing cloud-synced face profiles.
 *
 * Renders a gallery of known persons, supports search and selection, and shows
 * detail cards with avatars and metadata retrieved via ServerSyncManager.
 *
 * @example
 * auto* page = new FaceDatabasePage(sync, this);
 * page->setRemotePersons(persons);
 */

#include <QPixmap>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>
#include <QHash>
#include <QSet>
#include <QNetworkAccessManager>

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QPushButton;
class QLabel;
class ServerSyncManager;

#include "../../core/ServerTypes.h"

/**
 * @brief Gallery-style page for viewing and editing face database entries.
 */
class FaceDatabasePage : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief Constructs the face database UI and binds refresh controls.
     * @param sync Server sync manager providing person data.
     * @param parent Optional parent widget.
     */
    explicit FaceDatabasePage(ServerSyncManager* sync, QWidget* parent = nullptr);
    /**
     * @brief Updates the gallery with remotely fetched persons.
     * @param persons List of person records.
     */
    void setRemotePersons(const QList<PersonRecord>& persons);

signals:
    void requestCloudRefresh();

private slots:
    void handleSearchChanged(const QString& text);
    void handleSelectionChanged();
    void handleRename();
    void handleDelete();
    void handleNameEdited(const QString& text);
    void handleRoleEdited(const QString& text);
    void handleRoleUpdate();
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
    QLineEdit* roleEdit = nullptr;
    QPushButton* renameButton = nullptr;
    QPushButton* roleButton = nullptr;
    QPushButton* deleteButton = nullptr;
    QPushButton* refreshButton = nullptr;

    QList<PersonRecord> remotePersons;
    QString currentFilter;
    bool updatingSelection = false;
    QNetworkAccessManager* imageLoader = nullptr;
    QHash<QString, QPixmap> avatarCache;
    QSet<QString> pendingImages;

    void rebuildGallery();
    QStringList selectedIds() const;
    QString selectedPrimaryId() const;
    PersonRecord personById(const QString& id) const;
    void updateDetailPanel();
    QPixmap buildFacePixmap(const PersonRecord& person, const QSize& size);
    void setStatusMessage(const QString& text, bool isError = false);
    QString resolveImageKey(const QString& imageUrl) const;
    void ensureAvatarFetched(const PersonRecord& person);
    void refreshGalleryIcons(const QString& imageKey);
};

#endif // FACEDATABASEPAGE_H
