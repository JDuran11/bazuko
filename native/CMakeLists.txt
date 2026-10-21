cmake_minimum_required(VERSION 3.25)
project(bazuko VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencias antiguas (doctest) declaran un CMake mínimo que CMake 4 rechaza.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

option(BAZUKO_BUILD_APP "Compila el ejecutable con ventana (requiere SDL3)" ON)

FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.11
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(doctest)

if(BAZUKO_BUILD_APP)
  # SDL3: usa el paquete del sistema (dnf install SDL3-devel) o lo compila desde fuente.
  find_package(SDL3 3.2 QUIET CONFIG)
  if(NOT SDL3_FOUND)
    message(STATUS "SDL3 no encontrado en el sistema: se descarga con FetchContent")
    FetchContent_Declare(SDL3
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG release-3.2.28
      GIT_SHALLOW TRUE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL3)
  endif()
endif()

# Lógica sin dependencias de GL/SDL (testeable en consola).
add_library(bazuko_core STATIC src/core/FrameMetrics.cpp src/core/FrameLoop.cpp)
target_include_directories(bazuko_core PUBLIC src)
target_compile_options(bazuko_core PRIVATE -Wall -Wextra)

if(BAZUKO_BUILD_APP)
  add_executable(bazuko src/main.cpp)
  target_link_libraries(bazuko PRIVATE bazuko_core SDL3::SDL3)
  target_compile_options(bazuko PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_executable(bazuko_tests tests/core_test.cpp)
target_link_libraries(bazuko_tests PRIVATE bazuko_core doctest::doctest)
add_test(NAME bazuko_tests COMMAND bazuko_tests)
