cmake_minimum_required(VERSION 3.15)
project(voice_engine VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Debug/Release konfigürasyonları
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG_BUILD)
    message(STATUS "Debug build yapılandırıldı")
else()
    message(STATUS "Release build yapılandırıldı")
endif()

# Kütüphaneleri bul
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPUS REQUIRED opus)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

# Ana uygulama kaynak dosyaları
set(VOICE_ENGINE_SOURCES
        src/app/application.cpp
        src/app/main.cpp
        src/audio/audio_manager.cpp
        src/codec/opus_codec.cpp
        src/core/packet.cpp
        src/network/udp_receiver.cpp
        src/network/udp_sender.cpp
        src/processing/echo_canceller.cpp
        src/processing/noise_suppressor.cpp
        src/streaming/collector.cpp
        src/streaming/slicer.cpp
)

# Ana executable
add_executable(voice_engine ${VOICE_ENGINE_SOURCES})

target_include_directories(voice_engine PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OPUS_INCLUDE_DIRS}
        ${PORTAUDIO_INCLUDE_DIRS}
)

target_link_libraries(voice_engine PRIVATE
        ${OPUS_LIBRARIES}
        ${PORTAUDIO_LIBRARIES}
)

# Network test utility (opsiyonel)
add_executable(network_test
        src/tools/network_test.cpp
)

# Compiler uyarıları ve optimizasyonlar
if(NOT MSVC)
    target_compile_options(voice_engine PRIVATE
            -Wall -Wextra -Wpedantic
            $<$<CONFIG:Debug>:-g -O0 -DDEBUG>
            $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )
    target_compile_options(network_test PRIVATE
            -Wall -Wextra -Wpedantic
            $<$<CONFIG:Debug>:-g -O0>
            $<$<CONFIG:Release>:-O2>
    )
else()
    # MSVC için ayarlar
    target_compile_options(voice_engine PRIVATE
            /W4
            $<$<CONFIG:Debug>:/Od /Zi>
            $<$<CONFIG:Release>:/O2>
    )
    target_compile_options(network_test PRIVATE /W4)
endif()

# Windows için ek kütüphaneler
if(WIN32)
    target_link_libraries(voice_engine PRIVATE ws2_32)
    target_link_libraries(network_test PRIVATE ws2_32)
endif()

# Install kuralları
install(TARGETS voice_engine DESTINATION bin)
install(TARGETS network_test DESTINATION bin)

# CPack ayarları (paket oluşturma için)
set(CPACK_PACKAGE_NAME "NovaEngine-VoiceEngine")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Real-time P2P Voice Communication Engine")
set(CPACK_PACKAGE_VENDOR "NovaEngine")
include(CPack)

# Kullanışlı bilgiler
message(STATUS "====================================")
message(STATUS "NovaEngine Voice Engine v${PROJECT_VERSION}")
message(STATUS "====================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Opus found: ${OPUS_FOUND}")
message(STATUS "PortAudio found: ${PORTAUDIO_FOUND}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build targets:")
message(STATUS "  • voice_engine  - Ana ses iletişim uygulaması")
message(STATUS "  • network_test  - UDP bağlantı test aracı")
message(STATUS "====================================")

# Build sonrası mesajları - basit versiyon
add_custom_target(usage_info ALL
        COMMAND ${CMAKE_COMMAND} -E echo "Build tamamlandi!"
        DEPENDS voice_engine network_test
)