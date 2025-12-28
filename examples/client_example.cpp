/**
 * MCP Client Example
 * 
 * Demonstrates how to use the MCP client to connect to an MCP server
 * and call tools.
 */

#include <cppmcp/mcp_client.hpp>
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "MCP Client Example\n";
    std::cout << "==================\n\n";
    
    // Create client
    mcp::MCPClient client("example-client", "1.0.0");
    
    // Connect to server (SSE mode)
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <server-url>\n";
        std::cerr << "Example: " << argv[0] << " http://localhost:8080\n";
        return 1;
    }
    
    std::string url = argv[1];
    std::cout << "Connecting to " << url << "...\n";
    
    if (!client.connect_sse(url)) {
        std::cerr << "Failed to connect to server\n";
        return 1;
    }
    
    std::cout << "Connected!\n\n";
    
    // Initialize
    if (!client.initialize()) {
        std::cerr << "Failed to initialize\n";
        return 1;
    }
    
    std::cout << "Server: " << client.get_server_name() 
              << " v" << client.get_server_version() << "\n";
    std::cout << "Protocol: " << client.get_protocol_version() << "\n\n";
    
    // List available tools
    std::cout << "Available tools:\n";
    auto tools = client.list_tools();
    for (const auto& tool : tools) {
        std::cout << "  - " << tool.name << ": " << tool.description << "\n";
    }
    std::cout << "\n";
    
    // Call a tool
    if (!tools.empty()) {
        std::cout << "Calling tool '" << tools[0].name << "'...\n";
        
        json args = {
            {"a", 5},
            {"b", 3}
        };
        
        auto result = client.call_tool(tools[0].name, args);
        std::cout << "Result: " << result.dump(2) << "\n\n";
    }
    
    // List resources
    std::cout << "Available resources:\n";
    auto resources = client.list_resources();
    for (const auto& resource : resources) {
        std::cout << "  - " << resource.uri << ": " << resource.description << "\n";
    }
    std::cout << "\n";
    
    // Disconnect
    client.disconnect();
    std::cout << "Disconnected.\n";
    
    return 0;
}
