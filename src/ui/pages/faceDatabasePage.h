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
class QTableWidget;

#include "../../core/ServerTypes.h"
#include "../../core/AIProcessor.h"

class FaceDatabasePage : public QWidget
{
    Q_OBJECT
public:
    explicit FaceDatabasePage(AIProcessor* processor, QWidget* parent = nullptr);
    void setRemotePersons(const QList<PersonRecord>& persons);

signals:
    void requestCloudRefresh();

private slots:
    void refreshProfiles();
    void handleSearchChanged(const QString& text);
    void handleSelectionChanged();
    void handleRename();
    void handleDelete();
    void handleMerge();
    void handleNameEdited(const QString& text);
    void handleFaceDatabaseChanged();

private:
    AIProcessor* aiProcessor = nullptr;
    QLineEdit* searchInput = nullptr;
    QListWidget* gallery = nullptr;
    QLabel* previewLabel = nullptr;
    QLabel* samplesLabel = nullptr;
    QLabel* infoLabel = nullptr;
    QLabel* statusLabel = nullptr;
    QLineEdit* nameEdit = nullptr;
    QPushButton* renameButton = nullptr;
    QPushButton* deleteButton = nullptr;
    QPushButton* mergeButton = nullptr;
    QPushButton* refreshButton = nullptr;
    QTableWidget* remotePersonsTable = nullptr;
    QLabel* remoteStatusLabel = nullptr;
    QPushButton* remoteRefreshButton = nullptr;

    QVector<AIProcessor::FaceProfile> cachedProfiles;
    QString currentFilter;
    bool updatingSelection = false;
    QList<PersonRecord> remotePersons;

    QVector<AIProcessor::FaceProfile> fetchProfiles() const;
    void rebuildGallery();
    QStringList selectedIds() const;
    QString selectedPrimaryId() const;
    AIProcessor::FaceProfile profileForId(const QString& id) const;
    void updateDetailPanel();
    QPixmap buildFacePixmap(const AIProcessor::FaceProfile& profile, const QSize& size) const;
    void setStatusMessage(const QString& text, bool isError = false);
    void updateRemotePersonsTable();
};

#endif // FACEDATABASEPAGE_H
