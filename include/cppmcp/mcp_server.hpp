#ifndef MCP_SERVER_HPP
#define MCP_SERVER_HPP

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace mcp {

// Forward declarations
class MCPServer;

// Tool function signature
using ToolFunction = std::function<json(const json& arguments)>;

// Resource function signature
using ResourceFunction = std::function<std::string()>;

// Prompt function signature
using PromptFunction = std::function<json(const json& arguments)>;

// Tool definition
struct Tool {
    std::string name;
    std::string description;
    json input_schema;
    ToolFunction function;
};

// Resource definition
struct Resource {
    std::string uri;
    std::string name;
    std::string description;
    std::string mime_type;
    ResourceFunction function;
};

// Prompt definition
struct Prompt {
    std::string name;
    std::string description;
    json arguments;
    PromptFunction function;
};

// Transport mode
enum class TransportMode {
    STDIO,
    SSE
};

// MCP Server implementation
class MCPServer {
public:
    MCPServer(const std::string& name, const std::string& version = "1.0.0");
    ~MCPServer();

    // Register tools, resources, and prompts
    void add_tool(const std::string& name, const std::string& description,
                  const json& input_schema, ToolFunction func);
    
    void add_resource(const std::string& uri, const std::string& name,
                     const std::string& description, const std::string& mime_type,
                     ResourceFunction func);
    
    void add_prompt(const std::string& name, const std::string& description,
                   const json& arguments, PromptFunction func);

    // Run the server
    void run_stdio();
    void run_sse(int port = 8080);

    // Get server info
    std::string get_name() const { return server_name_; }
    std::string get_version() const { return server_version_; }

private:
    std::string server_name_;
    std::string server_version_;
    
    std::map<std::string, Tool> tools_;
    std::map<std::string, Resource> resources_;
    std::map<std::string, Prompt> prompts_;

    bool initialized_;
    json client_info_;

    // Message handling
    json handle_message(const json& message);
    json handle_initialize(const json& params);
    json handle_tools_list(const json& params);
    json handle_tools_call(const json& params);
    json handle_resources_list(const json& params);
    json handle_resources_read(const json& params);
    json handle_prompts_list(const json& params);
    json handle_prompts_get(const json& params);
    
    // Error responses
    json create_error_response(int id, int code, const std::string& message);
    json create_success_response(int id, const json& result);

    // STDIO transport
    void run_stdio_loop();
    std::string read_stdio_message();
    void write_stdio_message(const std::string& message);

    // SSE transport
    void run_sse_server(int port);
};

} // namespace mcp

#endif // MCP_SERVER_HPP
