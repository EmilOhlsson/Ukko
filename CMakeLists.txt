cmake_minimum_required(VERSION 3.27)

project(Ukko CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRE True)
set(CMAKE_EXPORT_COMPILE_COMMANDS True)

# add_compile_options(-Wall -Wextra -pedantic -Wshadow -Wconversion -Wcast-align -Weffc++)
add_compile_options(-Wall -Wextra -pedantic -Wcast-align)
add_compile_options("$<$<CONFIG:RELEASE>:-flto=auto>")
add_compile_options("$<$<CONFIG:DEBUG>:-Og;-g;-fno-omit-frame-pointer;-fsanitize=undefined,address>")
add_link_options("$<$<CONFIG:RELEASE>:-flto=auto>")
add_link_options("$<$<CONFIG:DEBUG>:-fsanitize=undefined,address>")

find_package(PkgConfig REQUIRED)
find_package(fmt REQUIRED)
find_package(CURL REQUIRED)

pkg_check_modules(GPIOD libgpiodcxx REQUIRED)
pkg_check_modules(CAIRO cairomm-1.0 REQUIRED)
pkg_check_modules(HTTPD libmicrohttpd REQUIRED)
pkg_check_modules(LUA lua REQUIRED)

add_executable(Ukko src/ukko.cpp src/forecast.cpp src/netatmo.cpp)

target_include_directories(Ukko PUBLIC
    ${GPIOD_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${LUA_INCLUDE_DIRS})
target_link_libraries(Ukko 
    ${LUA_LIBRARIES}
    ${CURL_LIBRARIES}
    ${GPIOD_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${HTTPD_LIBRARIES}
	fmt::fmt)

