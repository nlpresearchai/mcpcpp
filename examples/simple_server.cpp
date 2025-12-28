/**
 * Simple MCP Server Example
 * 
 * Demonstrates the basic usage of the cppmcp library to create
 * a minimal MCP server with a single tool.
 */

#include <cppmcp/mcp_server.hpp>
#include <iostream>

int main(int argc, char* argv[]) {
    // Create MCP server
    mcp::MCPServer server("simple-example", "1.0.0");
    
    // Add a simple calculator tool
    server.add_tool(
        "add",
        "Add two numbers together",
        {
            {"type", "object"},
            {"properties", {
                {"a", {{"type", "number"}, {"description", "First number"}}},
                {"b", {{"type", "number"}, {"description", "Second number"}}}
            }},
            {"required", {"a", "b"}}
        },
        [](const json& args) -> json {
            double a = args["a"];
            double b = args["b"];
            return {
                {"type", "text"},
                {"text", "Result: " + std::to_string(a + b)}
            };
        }
    );
    
    // Run server
    std::cout << "Starting simple MCP server...\n";
    std::cout << "Usage:\n";
    std::cout << "  STDIO mode: " << argv[0] << " stdio\n";
    std::cout << "  SSE mode:   " << argv[0] << " sse [port]\n\n";
    
    if (argc < 2) {
        std::cerr << "Error: Please specify transport mode (stdio or sse)\n";
        return 1;
    }
    
    std::string mode = argv[1];
    if (mode == "stdio") {
        server.run_stdio();
    } else if (mode == "sse") {
        int port = (argc > 2) ? std::stoi(argv[2]) : 8080;
        server.run_sse(port);
    } else {
        std::cerr << "Error: Unknown mode '" << mode << "'\n";
        return 1;
    }
    
    return 0;
}
