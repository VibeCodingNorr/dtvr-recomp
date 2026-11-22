cmake_minimum_required(VERSION 3.16)
project(dtvr_recomb VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2>=2.0.10)

add_library(dtvr-formats STATIC src/sbs.cpp)
target_include_directories(dtvr-formats PUBLIC src)
target_compile_features(dtvr-formats PUBLIC cxx_std_17)
target_compile_options(dtvr-formats PRIVATE -Wall -Wextra -Wpedantic)

add_executable(dtvr-recomb src/main.cpp)
target_compile_options(dtvr-recomb PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(dtvr-recomb PRIVATE dtvr-formats PkgConfig::SDL2)

enable_testing()
add_executable(sbs-test tests/sbs_test.cpp)
target_compile_options(sbs-test PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(sbs-test PRIVATE dtvr-formats)
add_test(NAME sbs COMMAND sbs-test)
install(TARGETS dtvr-recomb RUNTIME DESTINATION bin)
