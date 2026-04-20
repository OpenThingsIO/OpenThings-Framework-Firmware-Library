cmake_minimum_required(VERSION 3.10)
project(OpenThingsFramework CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Check for tiny_websockets (optional)
find_package(tiny_websockets QUIET)
if(tiny_websockets_FOUND)
    message(STATUS "Found tiny_websockets - WebSocket support enabled")
    set(HAVE_TINY_WEBSOCKETS ON)
else()
    message(STATUS "tiny_websockets not found - WebSocket support will be disabled")
    set(HAVE_TINY_WEBSOCKETS OFF)
endif()

# Add library source files (Linux only)
set(OTF_SOURCES
    etherport.cpp
    LinuxLocalServer.cpp
    OpenThingsFramework.cpp
    Request.cpp
    Response.cpp
    StringBuilder.cpp
)

# Note: Websocket.cpp and Websocket.h require tiny_websockets library
# For now, we compile only the core functionality without WebSocket support

# Create static library
add_library(openthings-framework STATIC ${OTF_SOURCES})

# Link dependencies
target_link_libraries(openthings-framework PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# Link WebSocket library if available
if(HAVE_TINY_WEBSOCKETS)
    target_link_libraries(openthings-framework PRIVATE tiny_websockets)
    target_compile_definitions(openthings-framework PRIVATE HAVE_TINY_WEBSOCKETS)
endif()

# Include directories
target_include_directories(openthings-framework PUBLIC .)

# Compiler flags for cleaner builds
if(MSVC)
    target_compile_options(openthings-framework PRIVATE /W4)
else()
    target_compile_options(openthings-framework PRIVATE 
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter  # Allow unused parameters in callbacks
    )
endif()

# Optional: Build example if compiled standalone
option(BUILD_EXAMPLE "Build example application" OFF)

if(BUILD_EXAMPLE)
    add_executable(example_server
        example/LinuxLocalServer_Config.h
        example/main.cpp
    )
    target_link_libraries(example_server PRIVATE openthings-framework OpenSSL::SSL OpenSSL::Crypto)
    target_include_directories(example_server PRIVATE .)
endif()

# Enable testing
enable_testing()

# Add compilation test
add_test(NAME compile_test COMMAND ${CMAKE_COMMAND} --build . --target openthings-framework)

message(STATUS "===== OpenThings Framework for Linux =====")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  OpenSSL: Found")
message(STATUS "  WebSocket: ${HAVE_TINY_WEBSOCKETS}")
message(STATUS "==========================================")

