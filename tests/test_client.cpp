// Basic client tests
#include <cppmcp/mcp_client.hpp>
#include <iostream>

int main() {
    std::cout << "Running client tests...\n";
    
    // Test 1: Client creation
    mcp::MCPClient client("test-client", "1.0.0");
    std::cout << "✓ Client created\n";
    
    std::cout << "\nAll tests passed!\n";
    return 0;
}
