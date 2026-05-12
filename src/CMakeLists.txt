# Header-only library: the HAL contract + mock implementation
add_library(tcommand_hal INTERFACE)

target_include_directories(tcommand_hal INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(tcommand_hal INTERFACE cxx_std_17)

# std::thread / std::mutex need pthread on Linux
find_package(Threads REQUIRED)
target_link_libraries(tcommand_hal INTERFACE Threads::Threads)