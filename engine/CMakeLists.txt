cmake_minimum_required(VERSION 3.28)

project(carwear VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CARWEAR_BUILD_TESTS "Build the test suite" ON)
option(CARWEAR_WERROR "Treat compiler warnings as errors" OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
    if(CARWEAR_WERROR)
        add_compile_options(/WX)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CARWEAR_WERROR)
        add_compile_options(-Werror)
    endif()
endif()

find_package(tomlplusplus CONFIG REQUIRED)

add_library(carwear_lib STATIC
    src/carwear/core/rng.cpp
    src/carwear/core/date.cpp
    src/carwear/config/loader.cpp
    src/carwear/config/serialize.cpp
    src/carwear/model/weather.cpp
    src/carwear/model/trips.cpp
    src/carwear/model/types.cpp
    src/carwear/model/physics.cpp
    src/carwear/model/state.cpp
    src/carwear/model/components.cpp
    src/carwear/model/maintenance.cpp
    src/carwear/model/failures.cpp
    src/carwear/model/simulation.cpp
    src/carwear/harness/statistics.cpp
    src/carwear/harness/monte_carlo.cpp
    src/carwear/output/csv.cpp
    src/carwear/output/run_writer.cpp
    src/carwear/output/aggregate_writer.cpp
    src/carwear/app/cli.cpp
)

target_include_directories(carwear_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(carwear_lib PUBLIC tomlplusplus::tomlplusplus)

add_executable(carwear src/main.cpp)
target_link_libraries(carwear PRIVATE carwear_lib)

if(CARWEAR_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory(tests)
endif()
