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
    
    # --- Основні файли, яких не вистачало ---
    ${CMAKE_SOURCE_DIR}/src/core/modelSettings.cpp
    ${CMAKE_SOURCE_DIR}/src/core/modelSettings.h
    ${CMAKE_SOURCE_DIR}/src/core/AppLogger.cpp
    ${CMAKE_SOURCE_DIR}/src/core/AppLogger.h
    ${CMAKE_SOURCE_DIR}/src/core/DetectionEventController.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DetectionEventController.h
    ${CMAKE_SOURCE_DIR}/src/core/ServerSyncManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ServerSyncManager.h
    ${CMAKE_SOURCE_DIR}/src/core/SupabaseClient.cpp
    ${CMAKE_SOURCE_DIR}/src/core/SupabaseClient.h
    
    # --- Залежності (AIProcessor, CameraManager тощо) ---
    ${CMAKE_SOURCE_DIR}/src/core/AIProcessor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/AIProcessor.h
    ${CMAKE_SOURCE_DIR}/src/core/AIProcessorONNX.cpp
    ${CMAKE_SOURCE_DIR}/src/core/AIProcessorONNX.h
    ${CMAKE_SOURCE_DIR}/src/core/faceReconstructionModel.cpp
    ${CMAKE_SOURCE_DIR}/src/core/faceReconstructionModel.h
    ${CMAKE_SOURCE_DIR}/src/core/cameraManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/cameraManager.h
    ${CMAKE_SOURCE_DIR}/src/core/FFVideoDecoder.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FFVideoDecoder.h
    ${CMAKE_SOURCE_DIR}/src/core/audioStreamPlayer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/audioStreamPlayer.h
    ${CMAKE_SOURCE_DIR}/src/core/RTSPCamera.cpp
    ${CMAKE_SOURCE_DIR}/src/core/RTSPCamera.h
    ${CMAKE_SOURCE_DIR}/src/core/ServerTypes.h
)
target_include_directories(core_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS}
    ${_extra_third_party_includes}
)
target_link_libraries(core_tests PRIVATE
    GTest::gtest
    Qt6::Core
    Qt6::Network
    Qt6::Gui
    Qt6::Multimedia
    ${OpenCV_LIBS}
    ${ONNX_RUNTIME_TARGET}
    ${FFMPEG_LIBRARIES}
)

if(WIN32)
    add_custom_command(TARGET core_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Qt6::Core>" "$<TARGET_FILE_DIR:core_tests>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Qt6::Gui>" "$<TARGET_FILE_DIR:core_tests>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Qt6::Network>" "$<TARGET_FILE_DIR:core_tests>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Qt6::Multimedia>" "$<TARGET_FILE_DIR:core_tests>"
    )
    # Копіюємо ONNX Runtime DLL, якщо використовується
    if (ORT_OV_DLLS)
        foreach(_dll ${ORT_OV_DLLS})
            add_custom_command(TARGET core_tests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "$<TARGET_FILE_DIR:core_tests>"
            )
        endforeach()
    endif()
endif()

gtest_discover_tests(core_tests DISCOVERY_TIMEOUT 60)
