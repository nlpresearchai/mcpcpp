cmake_minimum_required(VERSION 3.14)
project(cppmcp VERSION 1.0.0 LANGUAGES CXX)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(CPPMCP_BUILD_EXAMPLES "Build examples" ON)
option(CPPMCP_BUILD_TESTS "Build tests" ON)
option(CPPMCP_BUILD_SHARED "Build shared library" ON)
option(CPPMCP_BUILD_STATIC "Build static library" ON)

# Include FetchContent for dependencies
include(FetchContent)

# JSON library (nlohmann/json)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# HTTP library (cpp-httplib)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.1
)
FetchContent_MakeAvailable(httplib)

# Find libcurl
find_package(CURL REQUIRED)

# Source files
set(CPPMCP_SOURCES
    src/mcp_server.cpp
    src/mcp_sse.cpp
    src/mcp_client.cpp
    src/dynamic_mcp_server.cpp
)

set(CPPMCP_HEADERS
    include/cppmcp/mcp_server.hpp
    include/cppmcp/mcp_client.hpp
    include/cppmcp/dynamic_mcp_server.hpp
)

# Build shared library
if(CPPMCP_BUILD_SHARED)
    add_library(cppmcp SHARED ${CPPMCP_SOURCES})
    target_include_directories(cppmcp
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(cppmcp
        PRIVATE
            nlohmann_json::nlohmann_json
            httplib::httplib
            CURL::libcurl
    )
    set_target_properties(cppmcp PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
        PUBLIC_HEADER "${CPPMCP_HEADERS}"
    )
endif()

# Build static library
if(CPPMCP_BUILD_STATIC)
    add_library(cppmcp_static STATIC ${CPPMCP_SOURCES})
    target_include_directories(cppmcp_static
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(cppmcp_static
        PRIVATE
            nlohmann_json::nlohmann_json
            httplib::httplib
            CURL::libcurl
    )
    set_target_properties(cppmcp_static PROPERTIES
        OUTPUT_NAME cppmcp
        PUBLIC_HEADER "${CPPMCP_HEADERS}"
    )
    # Create an alias for easier building
    add_library(cppmcp::cppmcp ALIAS cppmcp_static)
endif()

# Examples
if(CPPMCP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(CPPMCP_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Install
include(GNUInstallDirs)

if(CPPMCP_BUILD_SHARED)
    install(TARGETS cppmcp
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cppmcp
    )
endif()

if(CPPMCP_BUILD_STATIC)
    install(TARGETS cppmcp_static
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cppmcp
    )
endif()
