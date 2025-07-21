# Add ONNX Runtime subdirectory FIRST
add_subdirectory(ThirdParty/ONNXRuntime)

# Add library src files to the library
file(GLOB SOURCES "src/*.cpp" "src/Managers/*.cpp")

# Create the library
add_library(strategies STATIC ${SOURCES})

# Configure include paths
target_include_directories(strategies
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Strategies
)

# Check if ONNX Runtime target exists and link accordingly
if(TARGET onnxruntime)
    message(STATUS "Linking strategies with ONNX Runtime")
    # Add spdlog and ONNX Runtime for headers that use them
    target_link_libraries(strategies PUBLIC 
        spdlog::spdlog
        onnxruntime
    )
else()
    message(WARNING "ONNX Runtime target not found, linking without it")
    # Add only spdlog
    target_link_libraries(strategies PUBLIC 
        spdlog::spdlog
    )
endif()