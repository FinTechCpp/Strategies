# Add library src files to the library
file(GLOB SOURCES "src/*.cpp")

# Create the library
add_library(strategies STATIC ${SOURCES})

# Configure include paths
target_include_directories(strategies
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Strategies
)

# Add spdlog for headers that use it
find_package(spdlog REQUIRED)
target_link_libraries(strategies PUBLIC spdlog::spdlog)