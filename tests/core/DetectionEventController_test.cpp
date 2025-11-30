#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QImage>
#include <QMetaObject>
#include <QSize>
#include <QTimer>
#include <QVector>
#include <QHash>

#include "core/AIProcessor.h"
#include "core/DetectionEventController.h"
#include "core/ServerSyncManager.h"
#include "core/ServerTypes.h"

namespace {

    void waitMs(int ms)
    {
        QEventLoop loop;
        QTimer::singleShot(ms, &loop, &QEventLoop::quit);
        loop.exec();
    }

    struct RecordedEvent {
        QString personId;
        int cameraId = -1;
        bool active = false;
        QDateTime timestamp;
    };

    class FakeServerSyncManager : public ServerSyncManager {
    public:
        explicit FakeServerSyncManager(const QHash<QString, QString>& nameMap, QObject* parent = nullptr)
            : ServerSyncManager(parent)
        {
            for (auto it = nameMap.constBegin(); it != nameMap.constEnd(); ++it)
                mapping.insert(it.key().toLower(), it.value());
        }

        QString personIdForName(const QString& name) const override
        {
            return mapping.value(name.toLower());
        }

        void sendDetectionStatus(const QString& personId,
            int cameraId,
            bool active,
            const QDateTime& timestamp,
            const QImage& snapshot, 
            float confidence)
            override
        {
            Q_UNUSED(snapshot);
            Q_UNUSED(confidence);
            events.push_back(RecordedEvent{ personId, cameraId, active, timestamp });
        }

        QVector<RecordedEvent> events;

    private:
        QHash<QString, QString> mapping;
    };

    Detection makeFace(const QString& label, const QString& category = QStringLiteral("Face"))
    {
        Detection d;
        d.label = label;
        d.category = category;
        d.confidence = 0.9f;
        d.rect = QRect(10, 10, 50, 50);
        return d;
    }

    void invokeHandleFrame(DetectionEventController& controller, int cameraId, const QVector<Detection>& detections)
    {

        QImage dummyFrame(640, 480, QImage::Format_RGB888);
        dummyFrame.fill(Qt::gray);

        QMetaObject::invokeMethod(&controller, "handleFrame", Qt::DirectConnection,
            Q_ARG(int, cameraId),
            Q_ARG(QImage, dummyFrame), 
            Q_ARG(QVector<Detection>, detections),
            Q_ARG(QSize, QSize(640, 480)));
    }
}

TEST(DetectionEventControllerTest, IgnoresNonFaceOrUnknownLabels)
{
    FakeServerSyncManager sync({ { "alice", "person-1" } });
    DetectionEventController controller(nullptr, &sync, nullptr, 200);

    invokeHandleFrame(controller, 1, { makeFace(QStringLiteral(""), QStringLiteral("Face")) });
    invokeHandleFrame(controller, 1, { makeFace(QStringLiteral("Face"), QStringLiteral("Face")) });
    invokeHandleFrame(controller, 1, { makeFace(QStringLiteral("Alice"), QStringLiteral("Object")) });
    invokeHandleFrame(controller, 1, { makeFace(QStringLiteral("Unknown"), QStringLiteral("Face")) });

    EXPECT_TRUE(sync.events.isEmpty());
}
