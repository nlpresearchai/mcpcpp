#include <cppmcp/mcp_server.hpp>
#include <iostream>
#include <sstream>
#include <thread>
#include <cstring>

namespace mcp {

MCPServer::MCPServer(const std::string& name, const std::string& version)
    : server_name_(name), server_version_(version), initialized_(false) {
}

MCPServer::~MCPServer() {
}

void MCPServer::add_tool(const std::string& name, const std::string& description,
                        const json& input_schema, ToolFunction func) {
    Tool tool;
    tool.name = name;
    tool.description = description;
    tool.input_schema = input_schema;
    tool.function = func;
    tools_[name] = tool;
}

void MCPServer::add_resource(const std::string& uri, const std::string& name,
                            const std::string& description, const std::string& mime_type,
                            ResourceFunction func) {
    Resource resource;
    resource.uri = uri;
    resource.name = name;
    resource.description = description;
    resource.mime_type = mime_type;
    resource.function = func;
    resources_[uri] = resource;
}

void MCPServer::add_prompt(const std::string& name, const std::string& description,
                          const json& arguments, PromptFunction func) {
    Prompt prompt;
    prompt.name = name;
    prompt.description = description;
    prompt.arguments = arguments;
    prompt.function = func;
    prompts_[name] = prompt;
}

json MCPServer::create_error_response(int id, int code, const std::string& message) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {
            {"code", code},
            {"message", message}
        }}
    };
}

json MCPServer::create_success_response(int id, const json& result) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", result}
    };
}

json MCPServer::handle_initialize(const json& params) {
    initialized_ = true;
    
    if (params.contains("clientInfo")) {
        client_info_ = params["clientInfo"];
    }

    // MCP protocol requires capabilities to be objects, not booleans
    json capabilities = json::object();
    
    if (!tools_.empty()) {
        capabilities["tools"] = json::object();
    }
    
    if (!resources_.empty()) {
        capabilities["resources"] = {
            {"subscribe", false},  // We don't support subscriptions yet
            {"listChanged", false}
        };
    }
    
    if (!prompts_.empty()) {
        capabilities["prompts"] = {
            {"listChanged", false}
        };
    }

    return {
        {"protocolVersion", "2024-11-05"},
        {"capabilities", capabilities},
        {"serverInfo", {
            {"name", server_name_},
            {"version", server_version_}
        }}
    };
}

json MCPServer::handle_tools_list(const json& params) {
    json tools_array = json::array();
    
    for (const auto& [name, tool] : tools_) {
        tools_array.push_back({
            {"name", tool.name},
            {"description", tool.description},
            {"inputSchema", tool.input_schema}
        });
    }
    
    return {{"tools", tools_array}};
}

json MCPServer::handle_tools_call(const json& params) {
    if (!params.contains("name")) {
        throw std::runtime_error("Missing 'name' parameter");
    }
    
    std::string tool_name = params["name"];
    
    if (tools_.find(tool_name) == tools_.end()) {
        throw std::runtime_error("Tool not found: " + tool_name);
    }
    
    json arguments = params.contains("arguments") ? params["arguments"] : json::object();
    
    try {
        json result = tools_[tool_name].function(arguments);
        
        // Format result according to MCP spec
        return {
            {"content", json::array({
                {
                    {"type", "text"},
                    {"text", result.is_string() ? result.get<std::string>() : result.dump()}
                }
            })}
        };
    } catch (const std::exception& e) {
        throw std::runtime_error("Tool execution failed: " + std::string(e.what()));
    }
}

json MCPServer::handle_resources_list(const json& params) {
    json resources_array = json::array();
    
    for (const auto& [uri, resource] : resources_) {
        resources_array.push_back({
            {"uri", resource.uri},
            {"name", resource.name},
            {"description", resource.description},
            {"mimeType", resource.mime_type}
        });
    }
    
    return {{"resources", resources_array}};
}

json MCPServer::handle_resources_read(const json& params) {
    if (!params.contains("uri")) {
        throw std::runtime_error("Missing 'uri' parameter");
    }
    
    std::string uri = params["uri"];
    
    if (resources_.find(uri) == resources_.end()) {
        throw std::runtime_error("Resource not found: " + uri);
    }
    
    try {
        std::string content = resources_[uri].function();
        
        return {
            {"contents", json::array({
                {
                    {"uri", uri},
                    {"mimeType", resources_[uri].mime_type},
                    {"text", content}
                }
            })}
        };
    } catch (const std::exception& e) {
        throw std::runtime_error("Resource read failed: " + std::string(e.what()));
    }
}

json MCPServer::handle_prompts_list(const json& params) {
    json prompts_array = json::array();
    
    for (const auto& [name, prompt] : prompts_) {
        prompts_array.push_back({
            {"name", prompt.name},
            {"description", prompt.description},
            {"arguments", prompt.arguments}
        });
    }
    
    return {{"prompts", prompts_array}};
}

json MCPServer::handle_prompts_get(const json& params) {
    if (!params.contains("name")) {
        throw std::runtime_error("Missing 'name' parameter");
    }
    
    std::string prompt_name = params["name"];
    
    if (prompts_.find(prompt_name) == prompts_.end()) {
        throw std::runtime_error("Prompt not found: " + prompt_name);
    }
    
    json arguments = params.contains("arguments") ? params["arguments"] : json::object();
    
    try {
        json result = prompts_[prompt_name].function(arguments);
        
        return {
            {"description", prompts_[prompt_name].description},
            {"messages", result}
        };
    } catch (const std::exception& e) {
        throw std::runtime_error("Prompt execution failed: " + std::string(e.what()));
    }
}

json MCPServer::handle_message(const json& message) {
    try {
        // Validate JSON-RPC 2.0 message
        if (!message.contains("jsonrpc") || message["jsonrpc"] != "2.0") {
            return create_error_response(-1, -32600, "Invalid JSON-RPC version");
        }
        
        if (!message.contains("method")) {
            return create_error_response(-1, -32600, "Missing method");
        }
        
        std::string method = message["method"];
        json params = message.contains("params") ? message["params"] : json::object();
        int id = message.contains("id") ? message["id"].get<int>() : -1;
        
        // Handle initialization
        if (method == "initialize") {
            json result = handle_initialize(params);
            return create_success_response(id, result);
        }
        
        // Check if initialized for other methods
        if (!initialized_ && method != "initialize") {
            return create_error_response(id, -32002, "Server not initialized");
        }
        
        // Route to appropriate handler
        json result;
        if (method == "tools/list") {
            result = handle_tools_list(params);
        } else if (method == "tools/call") {
            result = handle_tools_call(params);
        } else if (method == "resources/list") {
            result = handle_resources_list(params);
        } else if (method == "resources/read") {
            result = handle_resources_read(params);
        } else if (method == "prompts/list") {
            result = handle_prompts_list(params);
        } else if (method == "prompts/get") {
            result = handle_prompts_get(params);
        } else {
            return create_error_response(id, -32601, "Method not found: " + method);
        }
        
        return create_success_response(id, result);
        
    } catch (const json::exception& e) {
        return create_error_response(-1, -32700, "Parse error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        int id = message.contains("id") ? message["id"].get<int>() : -1;
        return create_error_response(id, -32603, "Internal error: " + std::string(e.what()));
    }
}

std::string MCPServer::read_stdio_message() {
    std::string line;
    std::getline(std::cin, line);
    return line;
}

void MCPServer::write_stdio_message(const std::string& message) {
    std::cout << message << std::endl;
    std::cout.flush();
}

void MCPServer::run_stdio_loop() {
    std::cerr << "MCP Server '" << server_name_ << "' starting in STDIO mode..." << std::endl;
    
    while (std::cin) {
        try {
            std::string input = read_stdio_message();
            
            if (input.empty()) {
                continue;
            }
            
            // Parse JSON
            json request = json::parse(input);
            
            // Handle message
            json response = handle_message(request);
            
            // Send response
            write_stdio_message(response.dump());
            
        } catch (const json::exception& e) {
            std::cerr << "JSON error: " << e.what() << std::endl;
            json error = create_error_response(-1, -32700, "Parse error");
            write_stdio_message(error.dump());
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
}

void MCPServer::run_stdio() {
    run_stdio_loop();
}

void MCPServer::run_sse(int port) {
    run_sse_server(port);
}

} // namespace mcp
