# Header-only library: HAL contract + Command pattern core
add_library(tcommand INTERFACE)

target_include_directories(tcommand INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(tcommand INTERFACE cxx_std_17)

# std::thread / std::mutex need pthread on Linux
find_package(Threads REQUIRED)
target_link_libraries(tcommand INTERFACE Threads::Threads)