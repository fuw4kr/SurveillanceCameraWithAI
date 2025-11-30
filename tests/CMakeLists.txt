enable_testing()

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)
find_package(OpenCV REQUIRED)

# Try to pick up vcpkg-installed headers (e.g., onnxruntime) used by project headers.
set(_vcpkg_triplet "${VCPKG_TARGET_TRIPLET}")
if (NOT _vcpkg_triplet OR _vcpkg_triplet STREQUAL "")
    set(_vcpkg_triplet "x64-windows")
endif()
set(_vcpkg_include "${CMAKE_BINARY_DIR}/vcpkg_installed/${_vcpkg_triplet}/include")
set(_extra_third_party_includes)
if (EXISTS "${_vcpkg_include}")
    list(APPEND _extra_third_party_includes "${_vcpkg_include}")
endif()
if (DEFINED ONNX_RUNTIME_INCLUDE_DIR AND ONNX_RUNTIME_INCLUDE_DIR AND EXISTS "${ONNX_RUNTIME_INCLUDE_DIR}")
    list(APPEND _extra_third_party_includes "${ONNX_RUNTIME_INCLUDE_DIR}")
endif()

# ============== Core tests ==============
add_executable(core_tests
    core/core_test_main.cpp
    core/ModelSettings_test.cpp
    core/AppLogger_test.cpp
    core/ServerTypes_test.cpp
    core/DetectionEventController_test.cpp
    core/ServerSyncManager_test.cpp
    core/SupabaseClient_test.cpp
    ${CMAKE_SOURCE_DIR}/src/core/modelSettings.cpp
    ${CMAKE_SOURCE_DIR}/src/core/AppLogger.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DetectionEventController.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ServerSyncManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/SupabaseClient.cpp
)
target_include_directories(core_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
    ${_extra_third_party_includes}
)
target_link_libraries(core_tests PRIVATE
    GTest::gtest
    Qt6::Core
    Qt6::Network
    Qt6::Gui
    ${OpenCV_LIBS}
)
gtest_discover_tests(core_tests)

# ============== UI: pages ==============
add_executable(ui_pages_tests
    ui/ui_test_main.cpp
    ui/pages/DashboardPage_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/pages/dashboardpage.cpp
)
target_include_directories(ui_pages_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(ui_pages_tests PRIVATE
    GTest::gtest
    Qt6::Widgets
    Qt6::Charts
    Qt6::Test
)
gtest_discover_tests(ui_pages_tests)

# ============== UI: widgets ==============
add_executable(ui_widgets_tests
    ui/ui_test_main.cpp
    ui/widgets/CameraViewWidget_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/widgets/cameraViewWidget.cpp
)
target_include_directories(ui_widgets_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(ui_widgets_tests PRIVATE
    GTest::gtest
    Qt6::Widgets
    Qt6::Gui
    Qt6::Test
)
gtest_discover_tests(ui_widgets_tests)

# ============== UI: windowEdit ==============
add_executable(ui_windowedit_tests
    ui/ui_test_main.cpp
    ui/windowEdit/SnapPreviewWindow_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/windowEdit/snapPreviewWindow.cpp
)
target_include_directories(ui_windowedit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(ui_windowedit_tests PRIVATE
    GTest::gtest
    Qt6::Widgets
    Qt6::Gui
    Qt6::Test
)
gtest_discover_tests(ui_windowedit_tests)

# ============== UI: controllers ==============
add_executable(ui_controllers_tests
    ui/ui_test_main.cpp
    ui/controllers/FaceAlertController_test.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/FaceAlertController.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/dialogs/UnknownFaceDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ServerSyncManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/SupabaseClient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/modelSettings.cpp
    ${CMAKE_SOURCE_DIR}/src/core/AppLogger.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DetectionEventController.cpp
)
target_include_directories(ui_controllers_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
    ${_extra_third_party_includes}
)
target_link_libraries(ui_controllers_tests PRIVATE
    GTest::gtest
    Qt6::Widgets
    Qt6::Gui
    Qt6::Network
    Qt6::Core
    Qt6::Test
    ${OpenCV_LIBS}
)
gtest_discover_tests(ui_controllers_tests)
