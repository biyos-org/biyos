add_library(biyos_internal STATIC
    core/check.cpp
    core/clock.cpp
    core/deadline.cpp
    core/environment.cpp
    core/error.cpp
    core/events.cpp
    core/format.cpp
    core/option_lookup.cpp
    core/option_table.cpp
    core/option_text.cpp
    core/option_warning.cpp
    core/options.cpp
    core/status.cpp
    core/timer.cpp
    core/tolerances.cpp
    core/utf8.cpp
    core/version.cpp
    model/checks.cpp
    model/entries.cpp
    model/model.cpp
    model/rows.cpp)

target_include_directories(biyos_internal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include)
target_compile_features(biyos_internal PUBLIC cxx_std_20)
target_compile_definitions(biyos_internal PRIVATE
    BIYOS_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    BIYOS_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    BIYOS_VERSION_PATCH=${PROJECT_VERSION_PATCH})
set_target_properties(biyos_internal PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
biyos_set_warnings(biyos_internal)

add_library(biyos
    capi/arrays.cpp
    capi/environment.cpp
    capi/events.cpp
    capi/format.cpp
    capi/model.cpp
    capi/options.cpp
    capi/rows.cpp
    capi/version.cpp)
add_library(biyos::biyos ALIAS biyos)

target_include_directories(biyos PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(biyos PRIVATE biyos_internal)
target_compile_definitions(biyos PRIVATE BIYOS_BUILDING)
if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(biyos PUBLIC BIYOS_STATIC)
endif()
set_target_properties(biyos PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR})
biyos_set_warnings(biyos)
