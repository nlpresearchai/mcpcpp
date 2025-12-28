#include <cppmcp/mcp_server.hpp>
#include <iostream>
#include <cmath>

int main(int argc, char* argv[]) {
    // Check transport mode
    std::string mode = "stdio";
    int port = 8080;
    
    if (argc > 1) {
        mode = argv[1];
        if (argc > 2) {
            port = std::atoi(argv[2]);
        }
    }
    
    // Create MCP server
    mcp::MCPServer server("cpp-example-server", "1.0.0");
    
    // Add tools
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
            double a = args["a"].get<double>();
            double b = args["b"].get<double>();
            return a + b;
        }
    );
    
    server.add_tool(
        "multiply",
        "Multiply two numbers",
        {
            {"type", "object"},
            {"properties", {
                {"a", {{"type", "number"}, {"description", "First number"}}},
                {"b", {{"type", "number"}, {"description", "Second number"}}}
            }},
            {"required", {"a", "b"}}
        },
        [](const json& args) -> json {
            double a = args["a"].get<double>();
            double b = args["b"].get<double>();
            return a * b;
        }
    );
    
    server.add_tool(
        "sqrt",
        "Calculate square root",
        {
            {"type", "object"},
            {"properties", {
                {"value", {{"type", "number"}, {"description", "Number to calculate square root"}}}
            }},
            {"required", {"value"}}
        },
        [](const json& args) -> json {
            double value = args["value"].get<double>();
            if (value < 0) {
                throw std::runtime_error("Cannot calculate square root of negative number");
            }
            return std::sqrt(value);
        }
    );
    
    server.add_tool(
        "greet",
        "Generate a greeting message",
        {
            {"type", "object"},
            {"properties", {
                {"name", {{"type", "string"}, {"description", "Name to greet"}}}
            }},
            {"required", {"name"}}
        },
        [](const json& args) -> json {
            std::string name = args["name"].get<std::string>();
            return "Hello, " + name + "! Welcome to C++ MCP Server!";
        }
    );
    
    // Add resources
    server.add_resource(
        "config://server",
        "server-config",
        "Server configuration information",
        "application/json",
        []() -> std::string {
            json config = {
                {"name", "cpp-example-server"},
                {"version", "1.0.0"},
                {"language", "C++"},
                {"features", {"tools", "resources", "prompts"}}
            };
            return config.dump(2);
        }
    );
    
    server.add_resource(
        "info://capabilities",
        "capabilities",
        "Server capabilities",
        "text/plain",
        []() -> std::string {
            return "C++ MCP Server Capabilities:\n"
                   "- Mathematical operations (add, multiply, sqrt)\n"
                   "- Greeting generation\n"
                   "- Configuration access\n"
                   "- Transport modes: STDIO and SSE";
        }
    );
    
    // Add prompts
    server.add_prompt(
        "math_tutor",
        "A helpful math tutor prompt",
        json::array({
            {{"name", "topic"}, {"description", "Math topic to teach"}, {"required", true}}
        }),
        [](const json& args) -> json {
            std::string topic = args["topic"].get<std::string>();
            return json::array({
                {
                    {"role", "user"},
                    {"content", {
                        {"type", "text"},
                        {"text", "Teach me about " + topic + " in a simple way"}
                    }}
                }
            });
        }
    );
    
    server.add_prompt(
        "code_helper",
        "Help with C++ programming",
        json::array({
            {{"name", "question"}, {"description", "Programming question"}, {"required", true}}
        }),
        [](const json& args) -> json {
            std::string question = args["question"].get<std::string>();
            return json::array({
                {
                    {"role", "user"},
                    {"content", {
                        {"type", "text"},
                        {"text", "I need help with C++ programming. Question: " + question}
                    }}
                }
            });
        }
    );
    
    // Run server based on mode
    if (mode == "sse" || mode == "http") {
        server.run_sse(port);
    } else {
        server.run_stdio();
    }
    
    return 0;
}
