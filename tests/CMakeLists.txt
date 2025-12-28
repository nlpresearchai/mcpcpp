# Tests

add_executable(test_server test_server.cpp)
target_link_libraries(test_server PRIVATE 
    cppmcp_static
    nlohmann_json::nlohmann_json
    httplib::httplib
    CURL::libcurl
    pthread
)

add_executable(test_client test_client.cpp)
target_link_libraries(test_client PRIVATE 
    cppmcp_static
    nlohmann_json::nlohmann_json
    httplib::httplib
    CURL::libcurl
    pthread
)

enable_testing()
add_test(NAME ServerTest COMMAND test_server)
add_test(NAME ClientTest COMMAND test_client)
