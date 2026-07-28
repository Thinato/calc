cmake_minimum_required(VERSION 3.24)
project(calc LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CALC_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(CALC_BUILD_TESTS "Build the test suite" ON)

add_library(calc_warnings INTERFACE)
target_compile_options(calc_warnings INTERFACE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
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
add_executable(calc src/main.cpp ${CALC_UI_SOURCES})
# ftxui::component transitively provides ftxui::dom and ftxui::screen.
target_link_libraries(calc PRIVATE calc_core calc_warnings ftxui::component)

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
