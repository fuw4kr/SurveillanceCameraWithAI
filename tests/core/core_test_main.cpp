#include <QCoreApplication>
#include <QMetaType>
#include <gtest/gtest.h>

#include "core/AIProcessor.h"
#include "core/ServerTypes.h"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    qRegisterMetaType<Detection>();
    qRegisterMetaType<QVector<Detection>>();
    qRegisterMetaType<PersonRecord>();
    qRegisterMetaType<QList<PersonRecord>>();
    qRegisterMetaType<CameraRecord>();
    qRegisterMetaType<QList<CameraRecord>>();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
