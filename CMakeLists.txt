cmake_minimum_required(VERSION 3.25)
project(SurveillanceCameraWithAI LANGUAGES CXX)

# ======================================================
# === Налаштування компілятора та Qt
# ======================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

set(OpenCV_ROOT "${VCPKG_INSTALLED_DIR}/x64-windows/share/opencv4")
    find_package(OpenCV REQUIRED)

find_package(Qt6 COMPONENTS Widgets Charts Network OpenGL Multimedia Svg REQUIRED)

# ======================================================
# === Файли проєкту
# ======================================================
set(SOURCES
    src/main.cpp
    src/ui/mainwindow.cpp
    src/ui/windowEdit/framelessWindow.cpp
    src/ui/windowEdit/snapPreviewWindow.cpp
 "src/ui/pages/dashboardpage.cpp")

set(HEADERS
    src/ui/mainwindow.h
    src/ui/windowEdit/framelessWindow.h
    src/ui/windowEdit/snapPreviewWindow.h
 "src/ui/pages/dashboardpage.h")

# ======================================================
# === Виконуваний файл
# ======================================================
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
)

# === DEPLOY Qt platform plugin ===
set(_QT_PLUGIN_PATH "C:/dev/vcpkg/packages/qtbase_x64-windows/debug/Qt6/plugins/platforms/qwindowsd.dll")

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${_QT_PLUGIN_PATH}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms/qwindows.dll"
)



# ======================================================
# === Лінкування бібліотек
# ======================================================
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::Svg
    Qt6::Network
    Qt6::OpenGL
    Qt6::Charts
    ${OpenCV_LIBS}
)

# ======================================================
# === Інформаційний вивід
# ======================================================
get_target_property(QtCore_location Qt6::Core LOCATION)
message(STATUS "==== Qt6 DETECTED ====")
message(STATUS "QtCore location: ${QtCore_location}")
message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
message(STATUS "CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")
message(STATUS "====================================================")
message(STATUS " Project:         ${PROJECT_NAME}")
message(STATUS " Qt6 Prefix:      ${CMAKE_PREFIX_PATH}")
message(STATUS " OpenCV Version:  ${OpenCV_VERSION}")
message(STATUS "====================================================")
