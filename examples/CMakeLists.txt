cmake_minimum_required(VERSION 3.25)

project(biyos_examples LANGUAGES C)

option(BIYOS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

find_package(biyos 0.1.0 EXACT CONFIG REQUIRED)

if(MSVC)
    set(warnings /W4 /permissive-)
    set(errors /WX)
else()
    set(warnings
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion)
    set(errors -Werror)
endif()
if(BIYOS_WARNINGS_AS_ERRORS)
    list(APPEND warnings ${errors})
endif()

enable_testing()

function(biyos_add_example name file)
    add_executable(example_${file} c/${file}.c)
    target_link_libraries(example_${file} PRIVATE biyos::biyos)
    set_target_properties(example_${file} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    target_compile_options(example_${file} PRIVATE ${warnings})
    add_test(NAME ExampleTest.${name} COMMAND example_${file})
    set_tests_properties(ExampleTest.${name} PROPERTIES
        ENVIRONMENT_MODIFICATION PATH=path_list_prepend:$<TARGET_FILE_DIR:biyos::biyos>)
endfunction()

biyos_add_example(Start start)
biyos_add_example(Model model)
biyos_add_example(Options options)
biyos_add_example(Events events)
biyos_add_example(Copies copies)
biyos_add_example(Errors errors)
