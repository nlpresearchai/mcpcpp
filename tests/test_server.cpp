// Basic server tests
#include <cppmcp/mcp_server.hpp>
#include <iostream>

int main() {
    std::cout << "Running server tests...\n";
    
    // Test 1: Server creation
    mcp::MCPServer server("test-server", "1.0.0");
    std::cout << "✓ Server created\n";
    
    // Test 2: Add tool
    server.add_tool("test_tool", "Test tool", {}, 
        [](const json& args) { return json{{"success", true}}; });
    std::cout << "✓ Tool added\n";
    
    // Test 3: Add resource
    server.add_resource("test://resource", "Test", "desc", "text/plain",
        []() { return "data"; });
    std::cout << "✓ Resource added\n";
    
    std::cout << "\nAll tests passed!\n";
    return 0;
}
