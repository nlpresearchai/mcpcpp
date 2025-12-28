# Examples

# Simple MCP Server
add_executable(simple_server simple_server.cpp)
target_link_libraries(simple_server PRIVATE 
    cppmcp_static
    nlohmann_json::nlohmann_json
    httplib::httplib
    CURL::libcurl
    pthread
)

# MCP Server with Tools
add_executable(server_with_tools server_with_tools.cpp)
target_link_libraries(server_with_tools PRIVATE 
    cppmcp_static
    nlohmann_json::nlohmann_json
    httplib::httplib
    CURL::libcurl
    pthread
)

# Dynamic MCP Server
add_executable(dynamic_server dynamic_server.cpp)
target_link_libraries(dynamic_server PRIVATE 
    cppmcp_static
    nlohmann_json::nlohmann_json
    httplib::httplib
    CURL::libcurl
    pthread
)

# MCP Client Example
add_executable(client_example client_example.cpp)
target_link_libraries(client_example PRIVATE 
    cppmcp_static
    nlohmann_json::nlohmann_json
    httplib::httplib
    CURL::libcurl
    pthread
)

# Install examples
install(TARGETS simple_server server_with_tools dynamic_server client_example
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
)
