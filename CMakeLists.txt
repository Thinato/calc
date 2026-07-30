cmake_minimum_required(VERSION 3.24)
project(calc LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CALC_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
# Off for a web build: the suite is a native binary that ctest runs, and there is
# no terminal-free reason to ship it through Emscripten.
set(CALC_BUILD_TESTS_DEFAULT ON)
if(EMSCRIPTEN)
  set(CALC_BUILD_TESTS_DEFAULT OFF)
endif()
option(CALC_BUILD_TESTS "Build the test suite" ${CALC_BUILD_TESTS_DEFAULT})
# Off by default: a warning should stop CI, not stop someone mid-edit on their own
# machine. Deliberately not CMAKE_COMPILE_WARNING_AS_ERROR, which is global and
# would also hold FTXUI to a standard this project does not set for it.
option(CALC_WERROR "Treat compiler warnings as errors (CI uses this)" OFF)

# What `calc --version` reports. The release workflow passes the tag it is
# building, so a downloaded binary can say which release it came from; a local
# build just uses the version declared above.
set(CALC_VERSION "${PROJECT_VERSION}" CACHE STRING "Version reported by calc --version")

add_library(calc_warnings INTERFACE)
target_compile_options(calc_warnings INTERFACE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
if(CALC_WERROR)
  # Only this target's users, which is only this project's own code.
  target_compile_options(calc_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Werror>
    $<$<CXX_COMPILER_ID:MSVC>:/WX>
  )
endif()
if(CALC_SANITIZE)
  target_compile_options(calc_warnings INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(calc_warnings INTERFACE -fsanitize=address,undefined)
endif()

# ---------------------------------------------------------------- dependencies
include(FetchContent)

set(FTXUI_BUILD_DOCS OFF CACHE INTERNAL "")
set(FTXUI_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(FTXUI_BUILD_TESTS OFF CACHE INTERNAL "")
set(FTXUI_ENABLE_INSTALL OFF CACHE INTERNAL "")
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG v7.0.1
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(ftxui)

if(EMSCRIPTEN)
  # FTXUI's cmake/ftxui_set_options.cmake appends "-s USE_PTHREADS" to
  # CMAKE_CXX_FLAGS in its own directory scope, for a threaded web build this
  # project deliberately does not want (see web/main.cpp). Our own objects never
  # see that flag, and wasm-ld refuses to link atomics against non-atomics, so the
  # two halves would not go together. Turn it back off for FTXUI's three
  # libraries: target options land after CMAKE_CXX_FLAGS on the command line, and
  # emcc takes the last -s setting it is given.
  foreach(ftxui_target screen dom component)
    target_compile_options(${ftxui_target} PRIVATE -sUSE_PTHREADS=0)
  endforeach()
endif()

if(CALC_BUILD_TESTS)
  FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.5.3
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(doctest)
endif()

# ------------------------------------------------------------------- calc_core
# Program logic: expression evaluation, the document model, the vim engine.
# Deliberately depends on nothing but the standard library so it is testable
# without a terminal.
file(GLOB_RECURSE CALC_CORE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/doc/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/vim/*.cpp"
)
add_library(calc_core STATIC ${CALC_CORE_SOURCES})
target_include_directories(calc_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(calc_core PRIVATE calc_warnings)

# ------------------------------------------------------------------------ calc
file(GLOB_RECURSE CALC_UI_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.cpp"
)
if(EMSCRIPTEN)
  # The buffer the page opens on, taken from the demo the repository already
  # ships so there is one copy of the tour rather than two.
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/examples/demo.calc" CALC_DEMO_TEXT)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/web/demo_buffer.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/web/demo_buffer.hpp"
    @ONLY
  )

  # Named calc.js so the pair of outputs is calc.js + calc.wasm, which is what
  # web/index.html loads.
  add_executable(calc_web web/main.cpp ${CALC_UI_SOURCES})
  set_target_properties(calc_web PROPERTIES OUTPUT_NAME calc SUFFIX ".js")
  target_include_directories(calc_web PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
  target_link_libraries(calc_web PRIVATE calc_core calc_warnings ftxui::component)
  target_link_options(calc_web PRIVATE
    # The heap starts small and the buffer can grow without a fixed ceiling.
    -sALLOW_MEMORY_GROWTH=1
    # The parser is recursive descent, and Emscripten's default 64KB stack is thin
    # for a visitor who types "((((((...".
    -sSTACK_SIZE=1MB
    # doc/file.cpp uses <fstream>, and the page installs stdin/stdout handlers
    # through FS.init, so the filesystem and FS itself both have to be present.
    -sFORCE_FILESYSTEM=1
    -sEXPORTED_RUNTIME_METHODS=FS
  )
else()
  add_executable(calc src/main.cpp ${CALC_UI_SOURCES})
  target_compile_definitions(calc PRIVATE CALC_VERSION_STRING="${CALC_VERSION}")
  # ftxui::component transitively provides ftxui::dom and ftxui::screen.
  target_link_libraries(calc PRIVATE calc_core calc_warnings ftxui::component)
endif()

# ------------------------------------------------------------------- calc_tests
if(CALC_BUILD_TESTS)
  enable_testing()
  file(GLOB_RECURSE CALC_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
  )
  add_executable(calc_tests ${CALC_TEST_SOURCES} ${CALC_UI_SOURCES})
  target_include_directories(calc_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  target_link_libraries(calc_tests
    PRIVATE calc_core calc_warnings doctest::doctest ftxui::component
  )
  include("${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")
  doctest_discover_tests(calc_tests)
endif()
