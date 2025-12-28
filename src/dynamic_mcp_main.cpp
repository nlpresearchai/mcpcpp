/*
 * Dynamic MCP Server
 * ==================
 * A C++ implementation of dynamic MCP server that reads task configurations
 * from a JSON file and dynamically creates MCP tools.
 * 
 * Usage:
 *   ./dynamic_mcp_server --config tasks_config.json --mode stdio
 *   ./dynamic_mcp_server --config tasks_config.json --mode sse --port 8080
 */

#include <cppmcp/dynamic_mcp_server.hpp>
#include <iostream>
#include <string>
#include <cstring>

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  --config FILE     Path to task configuration JSON file (required)\n"
              << "  --mode MODE       Transport mode: stdio or sse (default: stdio)\n"
              << "  --port PORT       Port for SSE mode (default: 8080)\n"
              << "  --host HOST       Host for SSE mode (default: 0.0.0.0)\n"
              << "  --help            Show this help message\n\n"
              << "Examples:\n"
              << "  " << program_name << " --config tasks_config.json\n"
              << "  " << program_name << " --config tasks_config.json --mode sse --port 8080\n"
              << std::endl;
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string config_path;
    std::string mode = "stdio";
    int port = 8080;
    std::string host = "0.0.0.0";
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        }
        else if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        }
        else if (arg == "--mode" && i + 1 < argc) {
            mode = argv[++i];
        }
        else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        }
        else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        }
        else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Validate required arguments
    if (config_path.empty()) {
        std::cerr << "Error: --config is required\n" << std::endl;
        print_usage(argv[0]);
        return 1;
    }
    
    if (mode != "stdio" && mode != "sse") {
        std::cerr << "Error: mode must be 'stdio' or 'sse'\n" << std::endl;
        return 1;
    }
    
    std::cerr << "======================================================================" << std::endl;
    std::cerr << "🚀 Dynamic MCP Server (C++) Starting" << std::endl;
    std::cerr << "======================================================================" << std::endl;
    std::cerr << "Config File: " << config_path << std::endl;
    std::cerr << "Transport:   " << mode << std::endl;
    if (mode == "sse") {
        std::cerr << "Host:        " << host << std::endl;
        std::cerr << "Port:        " << port << std::endl;
    }
    std::cerr << "======================================================================" << std::endl;
    
    try {
        // Load configuration
        dynamic_mcp::ConfigLoader config_loader(config_path);
        if (!config_loader.load()) {
            std::cerr << "❌ Failed to load configuration" << std::endl;
            return 1;
        }
        
        auto server_info = config_loader.get_server_info();
        std::string server_name = server_info.value("name", "DynamicTaskServer");
        
        // Create MCP server
        MCPServer mcp_server(server_name);
        
        // Add server info tool
        mcp_server.add_tool(
            "get_server_info",
            "Get server information and available tools",
            {
                {"type", "object"},
                {"properties", json::object()}
            },
            [&config_loader, server_info](const json& args) -> json {
                auto tasks = config_loader.get_tasks();
                auto workflows = config_loader.get_workflows();
                
                json task_list = json::array();
                for (const auto& task : tasks) {
                    task_list.push_back({
                        {"name", task.name},
                        {"type", task.operation_type}
                    });
                }
                
                json workflow_list = json::array();
                for (const auto& workflow : workflows) {
                    workflow_list.push_back({
                        {"name", workflow.name},
                        {"steps", workflow.steps.size()}
                    });
                }
                
                json result = server_info;
                result["task_count"] = tasks.size();
                result["workflow_count"] = workflows.size();
                result["tasks"] = task_list;
                result["workflows"] = workflow_list;
                
                return result;
            }
        );
        
        // Generate dynamic tools
        std::cerr << "\n📦 Generating Dynamic Tools..." << std::endl;
        std::cerr << "----------------------------------------------------------------------" << std::endl;
        
        dynamic_mcp::DynamicToolGenerator tool_generator(config_loader);
        tool_generator.generate_all_tools(mcp_server);
        
        std::cerr << "----------------------------------------------------------------------" << std::endl;
        std::cerr << "✅ Server initialized successfully" << std::endl;
        std::cerr << "======================================================================\n" << std::endl;
        
        // Run server based on mode
        if (mode == "stdio") {
            std::cerr << "Starting STDIO mode (reading from stdin, writing to stdout)..." << std::endl;
            mcp_server.run_stdio();
        }
        else if (mode == "sse") {
            std::cerr << "Starting SSE mode on " << host << ":" << port << "..." << std::endl;
            mcp_server.run_sse(port);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Server error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
