cmake_minimum_required (VERSION 3.10)
project (simplecpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11 CACHE STRING "C++ standard to use")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_STANDARD LESS 11)
    message(FATAL_ERROR "C++ standard was set to ${CMAKE_CXX_STANDARD} but 11 is required as a minimum")
endif()

include(CheckCXXCompilerFlag)

if (WIN32)
    # prevent simplifyPath_cppcheck() from wasting time on looking for a hypothetical network host
    add_definitions(-DUNCHOST=$ENV{COMPUTERNAME})
endif()

function(add_compile_options_safe FLAG)
    string(MAKE_C_IDENTIFIER "HAS_CXX_FLAG${FLAG}" mangled_flag)
    check_cxx_compiler_flag(${FLAG} ${mangled_flag})
    if (${mangled_flag})
        add_compile_options(${FLAG})
    endif()
endfunction()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-pedantic)

    add_compile_options(-Wall)
    add_compile_options(-Wextra)
    add_compile_options(-Wcast-qual)                # Cast for removing type qualifiers
    add_compile_options(-Wfloat-equal)              # Floating values used in equality comparisons
    add_compile_options(-Wmissing-declarations)     # If a global function is defined without a previous declaration
    add_compile_options(-Wmissing-format-attribute) #
    add_compile_options(-Wpacked)                   #
    add_compile_options(-Wredundant-decls)          # if anything is declared more than once in the same scope
    add_compile_options(-Wundef)
    add_compile_options(-Woverloaded-virtual)       # when a function declaration hides virtual functions from a base class

    add_compile_options(-Wsuggest-attribute=noreturn)
    if (NOT MINGW)
        add_compile_options_safe(-Wuseless-cast)
    endif()

    # we are not interested in these
    set_source_files_properties(test.cpp PROPERTIES COMPILE_FLAGS -Wno-multichar)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

    add_compile_options(/W4)  # Warning Level

    add_compile_options(/wd4127)  # warning C4127: conditional expression is constant
    add_compile_options(/wd4244)  # warning C4244: 'x': conversion from 'int' to 'char', possible loss of data
    add_compile_options(/wd4267)  # warning C4267: '...': conversion from 'size_t' to 'unsigned int', possible loss of data
    add_compile_options(/wd4706)  # warning C4706: assignment within conditional expression
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Weverything)

    # no need for c++98 compatibility
    add_compile_options(-Wno-c++98-compat-pedantic)

    # these are not really fixable until newer standards
    add_compile_options(-Wno-exit-time-destructors)
    add_compile_options(-Wno-global-constructors)
    add_compile_options(-Wno-weak-vtables)
    add_compile_options_safe(-Wno-unsafe-buffer-usage)
    add_compile_options_safe(-Wno-nrvo)

    # contradicts -Wcovered-switch-default
    add_compile_options(-Wno-switch-default)
    if (MINGW)
        add_compile_options(-Wno-reserved-macro-identifier)
        add_compile_options(-Wno-unused-macros)
    endif()

    # these are experimental warnings which might produce false positives
    add_compile_options_safe(-Wno-thread-safety-negative)
    add_compile_options_safe(-Wno-thread-safety-beta)

    # TODO: add [[clang::lifetimebound]] annotations?
    add_compile_options_safe(-Wno-lifetime-safety-intra-tu-suggestions)
    add_compile_options_safe(-Wno-lifetime-safety-intra-tu-constructor-suggestions)
    add_compile_options_safe(-Wno-lifetime-safety-cross-tu-constructor-suggestions)

    # TODO: fix these?
    add_compile_options(-Wno-padded)
    add_compile_options(-Wno-sign-conversion)
    add_compile_options(-Wno-implicit-int-conversion)
    add_compile_options(-Wno-shorten-64-to-32)
    add_compile_options(-Wno-shadow-field-in-constructor)

    # we are not interested in these
    set_source_files_properties(test.cpp PROPERTIES COMPILE_FLAGS "-Wno-multichar -Wno-four-char-constants")

    if (CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL 14 OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 14)
        # TODO: verify this regression still exists in clang-15
        if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
            # work around performance regression - see https://github.com/llvm/llvm-project/issues/53555
            add_compile_options(-mllvm -inline-deferral)
        endif()

        # use force DWARF 4 debug format since not all tools might be able to handle DWARF 5 yet - e.g. valgrind on ubuntu 20.04
        add_compile_options(-gdwarf-4)
    endif()
    if (APPLE)
        # CMake is sometimes chosing the wrong compiler on macos-* runners
        # see https://github.com/actions/runner/issues/4034
        add_compile_options(-Wno-poison-system-directories)
    endif()
endif()

add_library(simplecpp_obj OBJECT simplecpp.cpp)

add_executable(simplecpp $<TARGET_OBJECTS:simplecpp_obj> main.cpp)
add_executable(testrunner $<TARGET_OBJECTS:simplecpp_obj> test.cpp)
target_compile_definitions(testrunner
    PRIVATE
        SIMPLECPP_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

enable_testing()
add_test(NAME testrunner COMMAND testrunner)
