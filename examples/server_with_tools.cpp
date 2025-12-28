/**
 * MCP Server with Multiple Tools Example
 * 
 * Demonstrates how to create an MCP server with multiple tools,
 * resources, and prompts.
 */

#include <cppmcp/mcp_server.hpp>
#include <iostream>
#include <cmath>
#include <fstream>

int main(int argc, char* argv[]) {
    mcp::MCPServer server("tools-example", "1.0.0");
    
    // ========== Tools ==========
    
    // Math tool: Add
    server.add_tool(
        "add",
        "Add two numbers",
        {
            {"type", "object"},
            {"properties", {
                {"a", {{"type", "number"}}},
                {"b", {{"type", "number"}}}
            }},
            {"required", {"a", "b"}}
        },
        [](const json& args) -> json {
            double result = args["a"].get<double>() + args["b"].get<double>();
            return {{"result", result}};
        }
    );
    
    // Math tool: Multiply
    server.add_tool(
        "multiply",
        "Multiply two numbers",
        {
            {"type", "object"},
            {"properties", {
                {"a", {{"type", "number"}}},
                {"b", {{"type", "number"}}}
            }},
            {"required", {"a", "b"}}
        },
        [](const json& args) -> json {
            double result = args["a"].get<double>() * args["b"].get<double>();
            return {{"result", result}};
        }
    );
    
    // Math tool: Power
    server.add_tool(
        "power",
        "Calculate a^b",
        {
            {"type", "object"},
            {"properties", {
                {"base", {{"type", "number"}}},
                {"exponent", {{"type", "number"}}}
            }},
            {"required", {"base", "exponent"}}
        },
        [](const json& args) -> json {
            double result = std::pow(
                args["base"].get<double>(),
                args["exponent"].get<double>()
            );
            return {{"result", result}};
        }
    );
    
    // String tool: Uppercase
    server.add_tool(
        "uppercase",
        "Convert text to uppercase",
        {
            {"type", "object"},
            {"properties", {
                {"text", {{"type", "string"}}}
            }},
            {"required", {"text"}}
        },
        [](const json& args) -> json {
            std::string text = args["text"];
            for (auto& c : text) c = std::toupper(c);
            return {{"result", text}};
        }
    );
    
    // ========== Resources ==========
    
    server.add_resource(
        "config://app",
        "App Configuration",
        "Application configuration data",
        "application/json",
        []() -> std::string {
            json config = {
                {"version", "1.0.0"},
                {"features", {"tools", "resources", "prompts"}},
                {"max_connections", 100}
            };
            return config.dump(2);
        }
    );
    
    server.add_resource(
        "file://readme.txt",
        "README",
        "Application README",
        "text/plain",
        []() -> std::string {
            return "Welcome to cppmcp example server!\n"
                   "This server demonstrates multiple MCP capabilities.";
        }
    );
    
    // ========== Prompts ==========
    
    server.add_prompt(
        "code_review",
        "Generate a code review prompt",
        json::array({
            {
                {"name", "language"},
                {"description", "Programming language"},
                {"required", true}
            },
            {
                {"name", "focus"},
                {"description", "Review focus area"},
                {"required", false}
            }
        }),
        [](const json& args) -> json {
            std::string language = args["language"];
            std::string focus = args.value("focus", "general");
            
            return {
                {"messages", json::array({
                    {
                        {"role", "user"},
                        {"content", "Please review this " + language + 
                                   " code with focus on " + focus + " aspects."}
                    }
                })}
            };
        }
    );
    
    // ========== Run Server ==========
    
    std::cout << "MCP Server with Tools Example\n";
    std::cout << "==============================\n\n";
    std::cout << "Features:\n";
    std::cout << "  - 4 tools (add, multiply, power, uppercase)\n";
    std::cout << "  - 2 resources (config, readme)\n";
    std::cout << "  - 1 prompt (code_review)\n\n";
    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <stdio|sse> [port]\n";
        return 1;
    }
    
    std::string mode = argv[1];
    if (mode == "stdio") {
        std::cout << "Starting in STDIO mode...\n";
        server.run_stdio();
    } else if (mode == "sse") {
        int port = (argc > 2) ? std::stoi(argv[2]) : 8080;
        std::cout << "Starting SSE server on port " << port << "...\n";
        server.run_sse(port);
    } else {
        std::cerr << "Error: Unknown mode '" << mode << "'\n";
        return 1;
    }
    
    return 0;
}
