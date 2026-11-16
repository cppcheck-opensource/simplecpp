cmake_minimum_required (VERSION 3.10)
project (simplecpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11 CACHE STRING "C++ standard to use")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_STANDARD LESS 11)
    message(FATAL_ERROR "C++ standard was set to ${CMAKE_CXX_STANDARD} but 11 is required as a minimum")
endif()

include(CheckCXXCompilerFlag)

# compile options and definitions for all targets, they are applied with target_compile_options() and
# target_compile_definitions() since directory-level commands like add_compile_options() are not recommended
set(SIMPLECPP_COMPILE_OPTIONS)
set(SIMPLECPP_COMPILE_DEFINITIONS)

if (WIN32)
    # prevent simplifyPath_cppcheck() from wasting time on looking for a hypothetical network host
    list(APPEND SIMPLECPP_COMPILE_DEFINITIONS UNCHOST=$ENV{COMPUTERNAME})
endif()

function(add_compile_options_safe FLAG)
    string(MAKE_C_IDENTIFIER "HAS_CXX_FLAG${FLAG}" mangled_flag)
    check_cxx_compiler_flag(${FLAG} ${mangled_flag})
    if (${mangled_flag})
        set(SIMPLECPP_COMPILE_OPTIONS ${SIMPLECPP_COMPILE_OPTIONS} ${FLAG} PARENT_SCOPE)
    endif()
endfunction()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND SIMPLECPP_COMPILE_OPTIONS -pedantic)

    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wall)
    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wextra)
    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wcast-qual)                # Cast for removing type qualifiers
    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wfloat-equal)              # Floating values used in equality comparisons
    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wmissing-declarations)     # If a global function is defined without a previous declaration
    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wmissing-format-attribute) #
    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wpacked)                   #
    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wredundant-decls)          # if anything is declared more than once in the same scope
    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wundef)
    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Woverloaded-virtual)       # when a function declaration hides virtual functions from a base class

    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wsuggest-attribute=noreturn)
    if (NOT MINGW)
        add_compile_options_safe(-Wuseless-cast)
    endif()

    # we are not interested in these
    set_source_files_properties(test.cpp PROPERTIES COMPILE_FLAGS -Wno-multichar)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    list(APPEND SIMPLECPP_COMPILE_DEFINITIONS _CRT_SECURE_NO_WARNINGS)

    list(APPEND SIMPLECPP_COMPILE_OPTIONS /W4)  # Warning Level

    list(APPEND SIMPLECPP_COMPILE_OPTIONS /wd4127)  # warning C4127: conditional expression is constant
    list(APPEND SIMPLECPP_COMPILE_OPTIONS /wd4244)  # warning C4244: 'x': conversion from 'int' to 'char', possible loss of data
    list(APPEND SIMPLECPP_COMPILE_OPTIONS /wd4267)  # warning C4267: '...': conversion from 'size_t' to 'unsigned int', possible loss of data
    list(APPEND SIMPLECPP_COMPILE_OPTIONS /wd4706)  # warning C4706: assignment within conditional expression
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Weverything)

    # no need for c++98 compatibility
    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wno-c++98-compat-pedantic)

    # these are not really fixable until newer standards
    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wno-exit-time-destructors)
    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wno-global-constructors)
    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wno-weak-vtables)
    add_compile_options_safe(-Wno-unsafe-buffer-usage)
    add_compile_options_safe(-Wno-nrvo)

    # contradicts -Wcovered-switch-default
    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wno-switch-default)
    if (MINGW)
        list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wno-reserved-macro-identifier)
        list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wno-unused-macros)
    endif()

    # these are experimental warnings which might produce false positives
    add_compile_options_safe(-Wno-thread-safety-negative)
    add_compile_options_safe(-Wno-thread-safety-beta)

    # TODO: add [[clang::lifetimebound]] annotations?
    add_compile_options_safe(-Wno-lifetime-safety-intra-tu-suggestions)
    add_compile_options_safe(-Wno-lifetime-safety-intra-tu-constructor-suggestions)
    add_compile_options_safe(-Wno-lifetime-safety-cross-tu-constructor-suggestions)

    # TODO: fix these?
    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wno-padded)
    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wno-sign-conversion)
    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wno-implicit-int-conversion)
    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wno-shorten-64-to-32)
    list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wno-shadow-field-in-constructor)

    # we are not interested in these
    set_source_files_properties(test.cpp PROPERTIES COMPILE_FLAGS "-Wno-multichar -Wno-four-char-constants")

    if (CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL 14 OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 14)
        # TODO: verify this regression still exists in clang-15
        if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
            # work around performance regression - see https://github.com/llvm/llvm-project/issues/53555
            list(APPEND SIMPLECPP_COMPILE_OPTIONS -mllvm -inline-deferral)
        endif()

        # use force DWARF 4 debug format since not all tools might be able to handle DWARF 5 yet - e.g. valgrind on ubuntu 20.04
        list(APPEND SIMPLECPP_COMPILE_OPTIONS -gdwarf-4)
    endif()
    if (APPLE)
        # CMake is sometimes chosing the wrong compiler on macos-* runners
        # see https://github.com/actions/runner/issues/4034
        list(APPEND SIMPLECPP_COMPILE_OPTIONS -Wno-poison-system-directories)
    endif()
endif()

add_library(simplecpp_obj OBJECT simplecpp.cpp)

add_executable(simplecpp $<TARGET_OBJECTS:simplecpp_obj> main.cpp)
add_executable(testrunner $<TARGET_OBJECTS:simplecpp_obj> test.cpp)
foreach(target simplecpp_obj simplecpp testrunner)
    if (SIMPLECPP_COMPILE_OPTIONS)
        target_compile_options(${target} PRIVATE ${SIMPLECPP_COMPILE_OPTIONS})
    endif()
    if (SIMPLECPP_COMPILE_DEFINITIONS)
        target_compile_definitions(${target} PRIVATE ${SIMPLECPP_COMPILE_DEFINITIONS})
    endif()
endforeach()
target_compile_definitions(testrunner
    PRIVATE
        SIMPLECPP_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

enable_testing()
add_test(NAME testrunner COMMAND testrunner)
