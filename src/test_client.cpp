#include <cppmcp/mcp_client.hpp>
#include <iostream>
#include <iomanip>

using namespace mcp;

void print_separator(const std::string& title = "") {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    if (!title.empty()) {
        std::cout << title << std::endl;
        std::cout << std::string(70, '=') << std::endl;
    }
}

void print_subseparator(const std::string& title) {
    std::cout << "\n" << std::string(70, '-') << std::endl;
    std::cout << title << std::endl;
    std::cout << std::string(70, '-') << std::endl;
}

int main(int argc, char* argv[]) {
    std::string mode = "stdio";
    std::string server_cmd = "python";
    std::string server_script = "";
    std::string sse_url = "";
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc) {
            mode = argv[++i];
        } else if (arg == "--server" && i + 1 < argc) {
            server_script = argv[++i];
        } else if (arg == "--url" && i + 1 < argc) {
            sse_url = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --mode MODE      Transport mode: stdio or sse (default: stdio)\n";
            std::cout << "  --server SCRIPT  Python MCP server script path\n";
            std::cout << "  --url URL        SSE server URL (for sse mode)\n";
            std::cout << "  --help           Show this help\n";
            return 0;
        }
    }
    
    print_separator("C++ MCP CLIENT TEST");
    std::cout << "Transport: " << mode << std::endl;
    
    MCPClient client("cpp-mcp-test-client", "1.0.0");
    
    // Connect
    bool connected = false;
    if (mode == "stdio") {
        if (server_script.empty()) {
            std::cerr << "Error: --server required for stdio mode\n";
            std::cerr << "Example: --server examples/python_server.py\n";
            return 1;
        }
        connected = client.connect_stdio(server_cmd, {server_script});
    } else if (mode == "sse") {
        if (sse_url.empty()) {
            sse_url = "http://localhost:8181";
        }
        connected = client.connect_sse(sse_url);
    } else {
        std::cerr << "Unknown mode: " << mode << std::endl;
        return 1;
    }
    
    if (!connected) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }
    
    std::cout << "\nServer: " << client.get_server_name() 
              << " v" << client.get_server_version() << std::endl;
    std::cout << "Protocol: " << client.get_protocol_version() << std::endl;
    
    try {
        // List and call tools
        print_subseparator("Available Tools");
        auto tools = client.list_tools();
        std::cout << "Found " << tools.size() << " tools:" << std::endl;
        for (const auto& tool : tools) {
            std::cout << "  • " << tool.name << ": " << tool.description << std::endl;
        }
        
        // Test some tool calls
        print_subseparator("Testing Tools");
        
        // Try to find and call common tools
        for (const auto& tool : tools) {
            if (tool.name == "add") {
                json args = {{"a", 10}, {"b", 20}};
                auto result = client.call_tool("add", args);
                std::cout << "✓ add(10, 20) = " << result.dump() << std::endl;
            } else if (tool.name == "multiply") {
                json args = {{"a", 6}, {"b", 7}};
                auto result = client.call_tool("multiply", args);
                std::cout << "✓ multiply(6, 7) = " << result.dump() << std::endl;
            } else if (tool.name == "greet") {
                json args = {{"name", "C++ Client"}};
                auto result = client.call_tool("greet", args);
                std::cout << "✓ greet('C++ Client') = " << result.dump() << std::endl;
            }
        }
        
        // List resources
        print_subseparator("Available Resources");
        auto resources = client.list_resources();
        std::cout << "Found " << resources.size() << " resources:" << std::endl;
        for (const auto& resource : resources) {
            std::cout << "  • " << resource.uri << ": " << resource.name << std::endl;
        }
        
        // Read first resource
        if (!resources.empty()) {
            print_subseparator("Testing Resources");
            auto resource_data = client.read_resource(resources[0].uri);
            std::cout << "✓ Read " << resources[0].uri << std::endl;
            std::cout << "  Content: " << resource_data.dump(2).substr(0, 100) << "..." << std::endl;
        }
        
        // List prompts
        print_subseparator("Available Prompts");
        auto prompts = client.list_prompts();
        std::cout << "Found " << prompts.size() << " prompts:" << std::endl;
        for (const auto& prompt : prompts) {
            std::cout << "  • " << prompt.name << ": " << prompt.description << std::endl;
        }
        
        // Get first prompt
        if (!prompts.empty()) {
            print_subseparator("Testing Prompts");
            json args = {{"topic", "functions"}};
            auto prompt_data = client.get_prompt(prompts[0].name, args);
            std::cout << "✓ Got prompt: " << prompts[0].name << std::endl;
            if (prompt_data.contains("messages")) {
                std::cout << "  Messages: " << prompt_data["messages"].size() << std::endl;
            }
        }
        
        print_separator("✅ ALL TESTS COMPLETED SUCCESSFULLY");
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Error: " << e.what() << std::endl;
        client.disconnect();
        return 1;
    }
    
    client.disconnect();
    return 0;
}
