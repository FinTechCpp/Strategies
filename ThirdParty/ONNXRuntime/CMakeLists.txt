cmake_minimum_required(VERSION 3.14)

# Debug: Print current directory
message(STATUS "ONNX Runtime CMake: ${CMAKE_CURRENT_SOURCE_DIR}")

# Platform-specific library locations and naming
if(APPLE)
    set(ONNX_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/macos/libonnxruntime.1.22.0.dylib")
    message(STATUS "Looking for ONNX Runtime at: ${ONNX_LIB_PATH}")
    
    if(EXISTS "${ONNX_LIB_PATH}")
        message(STATUS "ONNX Runtime library found!")
        
        # Create an imported target for ONNX Runtime with GLOBAL scope
        add_library(onnxruntime SHARED IMPORTED GLOBAL)
        
        # Set the include directory
        target_include_directories(onnxruntime INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        
        # Set the library location
        set_target_properties(onnxruntime PROPERTIES
            IMPORTED_LOCATION "${ONNX_LIB_PATH}"
        )
        
        # Create an alias for consistent naming
        add_library(onnxruntime::onnxruntime ALIAS onnxruntime)
        message(STATUS "Created onnxruntime::onnxruntime alias target")
        
    else()
        message(FATAL_ERROR "ONNX Runtime library not found at: ${ONNX_LIB_PATH}")
    endif()
    
elseif(WIN32)
    set(ONNX_DLL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/windows/onnxruntime.dll")
    set(ONNX_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/windows/onnxruntime.lib")
    
    if(EXISTS "${ONNX_DLL_PATH}" AND EXISTS "${ONNX_LIB_PATH}")
        # Create an imported target for ONNX Runtime with GLOBAL scope
        add_library(onnxruntime SHARED IMPORTED GLOBAL)
        
        # Set the include directory
        target_include_directories(onnxruntime INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        
        # Set the library location
        set_target_properties(onnxruntime PROPERTIES
            IMPORTED_LOCATION "${ONNX_DLL_PATH}"
            IMPORTED_IMPLIB "${ONNX_LIB_PATH}"
        )
        
        # Create an alias for consistent naming
        add_library(onnxruntime::onnxruntime ALIAS onnxruntime)
        
    else()
        message(FATAL_ERROR "ONNX Runtime libraries not found. Expected: ${ONNX_DLL_PATH} and ${ONNX_LIB_PATH}")
    endif()
    
elseif(UNIX)
    set(ONNX_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/linux/libonnxruntime.so.1.22.0")
    
    if(EXISTS "${ONNX_LIB_PATH}")
        # Create an imported target for ONNX Runtime with GLOBAL scope
        add_library(onnxruntime SHARED IMPORTED GLOBAL)
        
        # Set the include directory
        target_include_directories(onnxruntime INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )
        
        # Set the library location
        set_target_properties(onnxruntime PROPERTIES
            IMPORTED_LOCATION "${ONNX_LIB_PATH}"
        )
        
        # Create an alias for consistent naming
        add_library(onnxruntime::onnxruntime ALIAS onnxruntime)
        
    else()
        message(FATAL_ERROR "ONNX Runtime library not found at: ${ONNX_LIB_PATH}")
    endif()
    
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()
