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

if (NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

find_package(OpenCV REQUIRED)


find_package(Qt6 COMPONENTS Widgets Charts Network OpenGL Multimedia Svg Concurrent REQUIRED)

find_package(FFMPEG REQUIRED)
include_directories(${FFMPEG_INCLUDE_DIRS})

set(_ffmpeg_triplet "${VCPKG_TARGET_TRIPLET}")
if (NOT _ffmpeg_triplet)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_ffmpeg_triplet "x64-windows")
    else()
        set(_ffmpeg_triplet "x86-windows")
    endif()
endif()

set(_ffmpeg_lib_hints
    "${CMAKE_BINARY_DIR}/vcpkg_installed/${_ffmpeg_triplet}/lib"
    "${CMAKE_BINARY_DIR}/vcpkg_installed/${_ffmpeg_triplet}/debug/lib"
)
if (DEFINED VCPKG_INSTALLED_DIR)
    list(APPEND _ffmpeg_lib_hints
        "${VCPKG_INSTALLED_DIR}/${_ffmpeg_triplet}/lib"
        "${VCPKG_INSTALLED_DIR}/${_ffmpeg_triplet}/debug/lib")
endif()

find_library(FFMPEG_SWRESAMPLE_LIBRARY
    NAMES swresample
    HINTS ${_ffmpeg_lib_hints}
)
find_library(FFMPEG_SWSCALE_LIBRARY
    NAMES swscale
    HINTS ${_ffmpeg_lib_hints}
)
if (NOT FFMPEG_SWRESAMPLE_LIBRARY OR NOT FFMPEG_SWSCALE_LIBRARY)
    message(FATAL_ERROR "Unable to locate FFmpeg swresample/swscale libraries. "
        "Set VCPKG_INSTALLED_DIR or install FFmpeg dev files.")
endif()
list(APPEND FFMPEG_LIBRARIES
    ${FFMPEG_SWRESAMPLE_LIBRARY}
    ${FFMPEG_SWSCALE_LIBRARY}
)

# ONNX Runtime: prefer OpenVINO execution provider on Windows (via NuGet package)
set(USE_ORT_OPENVINO ON CACHE BOOL "Fetch and use ONNX Runtime OpenVINO provider on Windows")
set(ONNX_RUNTIME_TARGET "")
set(ORT_OV_DLLS)

if (WIN32 AND USE_ORT_OPENVINO)
    set(ORT_OV_VERSION "1.23.0")
    set(ORT_OV_NUPKG "${CMAKE_BINARY_DIR}/onnxruntime-openvino-${ORT_OV_VERSION}.nupkg")
    set(ORT_OV_EXTRACT "${CMAKE_BINARY_DIR}/onnxruntime-openvino-${ORT_OV_VERSION}")
    set(ORT_OV_URL "https://www.nuget.org/api/v2/package/Intel.ML.OnnxRuntime.OpenVino/${ORT_OV_VERSION}")

    message(STATUS "Downloading ONNX Runtime OpenVINO package ${ORT_OV_VERSION}...")
    if (EXISTS "${ORT_OV_NUPKG}")
        file(REMOVE "${ORT_OV_NUPKG}")
    endif()
    file(DOWNLOAD "${ORT_OV_URL}" "${ORT_OV_NUPKG}" SHOW_PROGRESS STATUS ORT_OV_DL_STATUS)
    list(GET ORT_OV_DL_STATUS 0 ORT_OV_DL_CODE)
    if (NOT ORT_OV_DL_CODE EQUAL 0)
        message(FATAL_ERROR "Failed to download ${ORT_OV_URL}: ${ORT_OV_DL_STATUS}")
    endif()
    file(SIZE "${ORT_OV_NUPKG}" ORT_OV_SIZE)
    if (ORT_OV_SIZE EQUAL 0)
        message(FATAL_ERROR "Downloaded OpenVINO package is empty: ${ORT_OV_NUPKG}")
    endif()

    if (EXISTS "${ORT_OV_EXTRACT}")
        file(REMOVE_RECURSE "${ORT_OV_EXTRACT}")
    endif()
    file(MAKE_DIRECTORY "${ORT_OV_EXTRACT}")
    file(ARCHIVE_EXTRACT INPUT "${ORT_OV_NUPKG}" DESTINATION "${ORT_OV_EXTRACT}")

    set(ORT_OV_INCLUDE "${ORT_OV_EXTRACT}/build/native/include")
    set(ORT_OV_LIB_DIR "${ORT_OV_EXTRACT}/runtimes/win-x64/native")
    if (NOT EXISTS "${ORT_OV_INCLUDE}/onnxruntime_c_api.h")
        message(FATAL_ERROR "ONNX Runtime headers not found in ${ORT_OV_INCLUDE}")
    endif()
    if (NOT EXISTS "${ORT_OV_LIB_DIR}/onnxruntime.lib")
        message(FATAL_ERROR "ONNX Runtime library not found in ${ORT_OV_LIB_DIR}")
    endif()

    add_library(onnxruntime-openvino SHARED IMPORTED)
    set_target_properties(onnxruntime-openvino PROPERTIES
        IMPORTED_IMPLIB "${ORT_OV_LIB_DIR}/onnxruntime.lib"
        IMPORTED_LOCATION "${ORT_OV_LIB_DIR}/onnxruntime.dll"
    )
    target_include_directories(onnxruntime-openvino INTERFACE "${ORT_OV_INCLUDE}")
    set(ONNX_RUNTIME_TARGET onnxruntime-openvino)

    file(GLOB ORT_OV_DLLS "${ORT_OV_LIB_DIR}/*.dll")
else()
    if (NOT DEFINED VCPKG_TARGET_TRIPLET)
        set(VCPKG_TARGET_TRIPLET "x64-windows")
    endif()
    set(_ORT_VCPKG_ROOT "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")
    find_library(ONNXRUNTIME_LIB_RELEASE onnxruntime
        PATHS "${_ORT_VCPKG_ROOT}/lib"
        NO_DEFAULT_PATH
    )
    find_library(ONNXRUNTIME_LIB_DEBUG onnxruntime
        PATHS "${_ORT_VCPKG_ROOT}/debug/lib"
        NO_DEFAULT_PATH
    )
    set(ONNXRUNTIME_DLL_RELEASE "${_ORT_VCPKG_ROOT}/bin/onnxruntime.dll")
    set(ONNXRUNTIME_DLL_DEBUG "${_ORT_VCPKG_ROOT}/debug/bin/onnxruntime.dll")
    if (ONNXRUNTIME_LIB_RELEASE AND ONNXRUNTIME_LIB_DEBUG)
        add_library(onnxruntime-vcpkg SHARED IMPORTED)
        set_target_properties(onnxruntime-vcpkg PROPERTIES
            IMPORTED_IMPLIB "${ONNXRUNTIME_LIB_RELEASE}"
            IMPORTED_LOCATION "${ONNXRUNTIME_DLL_RELEASE}"
            IMPORTED_IMPLIB_DEBUG "${ONNXRUNTIME_LIB_DEBUG}"
            IMPORTED_LOCATION_DEBUG "${ONNXRUNTIME_DLL_DEBUG}"
        )
        target_include_directories(onnxruntime-vcpkg INTERFACE "${_ORT_VCPKG_ROOT}/include")
        set(ONNX_RUNTIME_TARGET onnxruntime-vcpkg)
    else()
        message(WARNING "onnxruntime libraries not found via vcpkg; AI features may fail to link")
    endif()
endif()
#1> [CMake]   find_package(FFMPEG REQUIRED)
#1> [CMake]   target_include_directories(main PRIVATE ${FFMPEG_INCLUDE_DIRS})
#1> [CMake]   target_link_directories(main PRIVATE ${FFMPEG_LIBRARY_DIRS})
#1> [CMake]   target_link_libraries(main PRIVATE ${FFMPEG_LIBRARIES})

# ======================================================
# === Файли проєкту
# ======================================================
set(SOURCES
    src/main.cpp
    src/resources.qrc
    src/ui/mainwindow.cpp
    src/ui/windowEdit/framelessWindow.cpp
    src/ui/windowEdit/snapPreviewWindow.cpp
    src/ui/pages/dashboardpage.cpp
    src/ui/pages/camerasPage.cpp
    src/ui/pages/analyticsPage.cpp
    src/ui/pages/faceDatabasePage.cpp
    src/ui/widgets/cameraViewWidget.cpp

    
    src/core/cameraManager.cpp
    src/core/AIProcessor.cpp
    src/core/AIProcessorONNX.cpp
    src/core/audioStreamPlayer.cpp
    src/core/FFVideoDecoder.cpp

)

set(HEADERS
    src/ui/mainwindow.h
    src/ui/windowEdit/framelessWindow.h
    src/ui/windowEdit/snapPreviewWindow.h
    src/ui/pages/dashboardpage.h
    src/ui/pages/camerasPage.h
    src/ui/pages/analyticsPage.h
    src/ui/pages/faceDatabasePage.h
    
    src/core/cameraManager.h
    src/core/AIProcessor.h
    src/core/AIProcessorONNX.h
    src/core/audioStreamPlayer.h
    src/core/FFVideoDecoder.h
    src/ui/widgets/cameraViewWidget.h
)

# ======================================================
# === Виконуваний файл
# ======================================================
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
)

if (WIN32 AND TARGET Qt6::QWindowsIntegrationPlugin)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms/$<TARGET_FILE_NAME:Qt6::QWindowsIntegrationPlugin>"
    )
endif()

if (ORT_OV_DLLS)
    foreach(_dll ${ORT_OV_DLLS})
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_dll}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            COMMENT "Copying ${_dll} to output directory"
        )
    endforeach()
endif()



# ======================================================
# === Лінкування бібліотек
# ======================================================
target_include_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Multimedia
    Qt6::Svg
    Qt6::Network
    Qt6::OpenGL
    Qt6::Charts
    ${OpenCV_LIBS}
    ${FFMPEG_LIBRARIES}
    ${ONNX_RUNTIME_TARGET}
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
