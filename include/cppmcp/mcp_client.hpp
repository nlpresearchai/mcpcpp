#pragma once

#include <string>
#include <functional>
#include <nlohmann/json.hpp>
#include <memory>
#include <vector>

using json = nlohmann::json;

namespace mcp {

// Forward declarations
struct Tool;
struct Resource;
struct Prompt;

/**
 * MCP Client for connecting to MCP servers
 * Supports both STDIO and SSE transports
 */
class MCPClient {
public:
    MCPClient(const std::string& name = "cpp-mcp-client", const std::string& version = "1.0.0");
    ~MCPClient();

    // Connection methods
    bool connect_stdio(const std::string& command, const std::vector<std::string>& args = {});
    bool connect_sse(const std::string& url);
    void disconnect();
    bool is_connected() const { return connected_; }

    // MCP Protocol methods
    bool initialize();
    std::vector<Tool> list_tools();
    json call_tool(const std::string& name, const json& arguments);
    std::vector<Resource> list_resources();
    json read_resource(const std::string& uri);
    std::vector<Prompt> list_prompts();
    json get_prompt(const std::string& name, const json& arguments);

    // Utility methods
    std::string get_server_name() const { return server_name_; }
    std::string get_server_version() const { return server_version_; }
    std::string get_protocol_version() const { return protocol_version_; }

private:
    json send_request(const std::string& method, const json& params = json::object());
    json read_response();
    void write_request(const json& request);
    
    std::string client_name_;
    std::string client_version_;
    std::string server_name_;
    std::string server_version_;
    std::string protocol_version_;
    
    bool connected_;
    int request_id_;
    
    // Transport specific
    enum class TransportType { STDIO, SSE };
    TransportType transport_type_;
    
    // STDIO transport
    int process_pid_;
    int stdin_fd_;
    int stdout_fd_;
    
    // SSE transport
    std::string sse_url_;
    std::string sse_endpoint_;
};

// Tool definition
struct Tool {
    std::string name;
    std::string description;
    json input_schema;
};

// Resource definition
struct Resource {
    std::string uri;
    std::string name;
    std::string description;
    std::string mime_type;
};

// Prompt definition
struct Prompt {
    std::string name;
    std::string description;
    json arguments;
};

} // namespace mcp
