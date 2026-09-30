cmake_minimum_required(VERSION 3.20)

project(GTA_SA_AudioDeviceRedirect LANGUAGES CXX)

add_library(AudioDeviceRedirect SHARED
    src/main.cpp
)

target_compile_features(AudioDeviceRedirect PRIVATE cxx_std_17)

target_compile_definitions(AudioDeviceRedirect PRIVATE
    UNICODE
    _UNICODE
)

target_link_libraries(AudioDeviceRedirect PRIVATE
    ole32
    runtimeobject
)

set_target_properties(AudioDeviceRedirect PROPERTIES
    OUTPUT_NAME "AudioDeviceRedirect"
    SUFFIX ".asi"
)

if(MSVC)
    target_compile_options(AudioDeviceRedirect PRIVATE
        /W4
        /EHsc
        /O2
    )
endif()
