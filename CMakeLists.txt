cmake_minimum_required(VERSION 3.25)
project(SurveillanceCameraWithAI LANGUAGES CXX)

# ======================================================
# === Основні налаштування
# ======================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# ======================================================
# === Шляхи до Qt
# ======================================================
set(CMAKE_PREFIX_PATH "C:/Qt/6.10.0/mingw_64")
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# ======================================================
# === Шляхи до OpenCV
# ======================================================
set(OpenCV_DIR "C:/dev/opencv/install/")

find_package(OpenCV REQUIRED)

message(STATUS "✅ OpenCV version: ${OpenCV_VERSION}")
message(STATUS "✅ OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "✅ OpenCV libraries: ${OpenCV_LIBS}")

include_directories(${OpenCV_INCLUDE_DIRS})

# ======================================================
# === Файли проєкту
# ======================================================
set(SOURCES
        main.cpp
        mainwindow.cpp
        windowEdit/framelessWindow.cpp
        windowEdit/snapPreviewWindow.cpp
)

set(HEADERS
        mainwindow.h
        windowEdit/framelessWindow.h
        windowEdit/snapPreviewWindow.h
)

set(UI
        mainwindow.ui
)

add_executable(${PROJECT_NAME}
        ${SOURCES}
        ${HEADERS}
        ${UI}
)

# ======================================================
# === Лінкування бібліотек
# ======================================================
target_link_libraries(${PROJECT_NAME}
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        ${OpenCV_LIBS}
)

# ======================================================
# === Автоматичне копіювання Qt DLL через windeployqt
# ======================================================
if (WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
            HINTS "${CMAKE_PREFIX_PATH}/bin"
    )
    if (WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-compiler-runtime "$<TARGET_FILE:${PROJECT_NAME}>"
                COMMENT "🚀 Running windeployqt to copy Qt dependencies..."
        )
    else()
        message(WARNING "⚠️ windeployqt not found — check your Qt installation.")
    endif()
endif()

# ======================================================
# === Інформаційний вивід
# ======================================================
message(STATUS "====================================================")
message(STATUS " Project:         ${PROJECT_NAME}")
message(STATUS " Qt Prefix Path:  ${CMAKE_PREFIX_PATH}")
message(STATUS " OpenCV Dir:      ${OpenCV_DIR}")
message(STATUS "====================================================")
