#include <cppmcp/mcp_client.hpp>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <curl/curl.h>
#include <cstring>

namespace mcp {

// Helper for CURL responses
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

MCPClient::MCPClient(const std::string& name, const std::string& version)
    : client_name_(name)
    , client_version_(version)
    , connected_(false)
    , request_id_(0)
    , process_pid_(-1)
    , stdin_fd_(-1)
    , stdout_fd_(-1)
    , transport_type_(TransportType::STDIO) {
}

MCPClient::~MCPClient() {
    disconnect();
}

bool MCPClient::connect_stdio(const std::string& command, const std::vector<std::string>& args) {
    std::cerr << "Connecting to MCP server via STDIO: " << command << std::endl;
    
    int stdin_pipe[2];
    int stdout_pipe[2];
    
    if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0) {
        std::cerr << "Failed to create pipes" << std::endl;
        return false;
    }
    
    process_pid_ = fork();
    
    if (process_pid_ < 0) {
        std::cerr << "Failed to fork process" << std::endl;
        return false;
    }
    
    if (process_pid_ == 0) {
        // Child process
        close(stdin_pipe[1]);   // Close write end
        close(stdout_pipe[0]);  // Close read end
        
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        
        // Prepare arguments
        std::vector<char*> exec_args;
        exec_args.push_back(const_cast<char*>(command.c_str()));
        for (const auto& arg : args) {
            exec_args.push_back(const_cast<char*>(arg.c_str()));
        }
        exec_args.push_back(nullptr);
        
        execvp(command.c_str(), exec_args.data());
        std::cerr << "Failed to exec: " << command << std::endl;
        exit(1);
    }
    
    // Parent process
    close(stdin_pipe[0]);   // Close read end
    close(stdout_pipe[1]);  // Close write end
    
    stdin_fd_ = stdin_pipe[1];
    stdout_fd_ = stdout_pipe[0];
    
    transport_type_ = TransportType::STDIO;
    connected_ = true;
    
    return initialize();
}

bool MCPClient::connect_sse(const std::string& url) {
    std::cerr << "Connecting to MCP server via SSE: " << url << std::endl;
    
    transport_type_ = TransportType::SSE;
    
    // For SSE (2024-11-05 protocol), the URL should be the base server URL
    // The endpoint event will tell us where to POST messages
    // Expected format when we GET /: "event: endpoint\ndata: /message\n\n"
    
    // Extract base URL and path
    size_t proto_end = url.find("://");
    if (proto_end == std::string::npos) {
        std::cerr << "Invalid URL format" << std::endl;
        return false;
    }
    
    size_t path_start = url.find("/", proto_end + 3);
    if (path_start != std::string::npos) {
        sse_url_ = url.substr(0, path_start);  // Base URL without path
    } else {
        sse_url_ = url;
    }
    
    // For now, assume the standard endpoint structure for 2024-11-05 protocol
    // The server should have sent "event: endpoint\ndata: /message\n\n" on GET /
    // But we'll just use the known endpoint directly
    sse_endpoint_ = "/message";
    
    std::cerr << "✓ SSE base URL: " << sse_url_ << std::endl;
    std::cerr << "✓ POST endpoint: " << sse_endpoint_ << std::endl;
    
    connected_ = true;
    return initialize();
}

void MCPClient::disconnect() {
    if (!connected_) return;
    
    if (transport_type_ == TransportType::STDIO) {
        if (stdin_fd_ >= 0) close(stdin_fd_);
        if (stdout_fd_ >= 0) close(stdout_fd_);
        
        if (process_pid_ > 0) {
            kill(process_pid_, SIGTERM);
            waitpid(process_pid_, nullptr, 0);
        }
    }
    
    connected_ = false;
    std::cerr << "Disconnected from MCP server" << std::endl;
}

bool MCPClient::initialize() {
    json params = {
        {"protocolVersion", "2024-11-05"},
        {"capabilities", json::object()},
        {"clientInfo", {
            {"name", client_name_},
            {"version", client_version_}
        }}
    };
    
    json response = send_request("initialize", params);
    
    if (response.contains("error")) {
        std::cerr << "Initialize failed: " << response["error"].dump() << std::endl;
        return false;
    }
    
    if (response.contains("result")) {
        auto result = response["result"];
        
        if (result.contains("serverInfo")) {
            server_name_ = result["serverInfo"]["name"].get<std::string>();
            server_version_ = result["serverInfo"]["version"].get<std::string>();
        }
        
        if (result.contains("protocolVersion")) {
            protocol_version_ = result["protocolVersion"].get<std::string>();
        }
        
        std::cerr << "✓ Connected to: " << server_name_ << " v" << server_version_ << std::endl;
        std::cerr << "✓ Protocol: " << protocol_version_ << std::endl;
        
        // Send initialized notification
        json notification = {
            {"jsonrpc", "2.0"},
            {"method", "notifications/initialized"}
        };
        write_request(notification);
        
        return true;
    }
    
    return false;
}

std::vector<Tool> MCPClient::list_tools() {
    json response = send_request("tools/list");
    std::vector<Tool> tools;
    
    if (response.contains("result") && response["result"].contains("tools")) {
        for (const auto& tool_json : response["result"]["tools"]) {
            Tool tool;
            tool.name = tool_json["name"].get<std::string>();
            tool.description = tool_json.value("description", "");
            tool.input_schema = tool_json.value("inputSchema", json::object());
            tools.push_back(tool);
        }
    }
    
    return tools;
}

json MCPClient::call_tool(const std::string& name, const json& arguments) {
    json params = {
        {"name", name},
        {"arguments", arguments}
    };
    
    json response = send_request("tools/call", params);
    
    if (response.contains("result")) {
        return response["result"];
    }
    
    return response;
}

std::vector<Resource> MCPClient::list_resources() {
    json response = send_request("resources/list");
    std::vector<Resource> resources;
    
    if (response.contains("result") && response["result"].contains("resources")) {
        for (const auto& res_json : response["result"]["resources"]) {
            Resource resource;
            resource.uri = res_json["uri"].get<std::string>();
            resource.name = res_json.value("name", "");
            resource.description = res_json.value("description", "");
            resource.mime_type = res_json.value("mimeType", "");
            resources.push_back(resource);
        }
    }
    
    return resources;
}

json MCPClient::read_resource(const std::string& uri) {
    json params = {
        {"uri", uri}
    };
    
    json response = send_request("resources/read", params);
    
    if (response.contains("result")) {
        return response["result"];
    }
    
    return response;
}

std::vector<Prompt> MCPClient::list_prompts() {
    json response = send_request("prompts/list");
    std::vector<Prompt> prompts;
    
    if (response.contains("result") && response["result"].contains("prompts")) {
        for (const auto& prompt_json : response["result"]["prompts"]) {
            Prompt prompt;
            prompt.name = prompt_json["name"].get<std::string>();
            prompt.description = prompt_json.value("description", "");
            prompt.arguments = prompt_json.value("arguments", json::array());
            prompts.push_back(prompt);
        }
    }
    
    return prompts;
}

json MCPClient::get_prompt(const std::string& name, const json& arguments) {
    json params = {
        {"name", name},
        {"arguments", arguments}
    };
    
    json response = send_request("prompts/get", params);
    
    if (response.contains("result")) {
        return response["result"];
    }
    
    return response;
}

json MCPClient::send_request(const std::string& method, const json& params) {
    json request = {
        {"jsonrpc", "2.0"},
        {"id", ++request_id_},
        {"method", method}
    };
    
    if (!params.is_null() && !params.empty()) {
        request["params"] = params;
    }
    
    if (transport_type_ == TransportType::STDIO) {
        write_request(request);
        return read_response();
    } else if (transport_type_ == TransportType::SSE) {
        // For SSE, do POST request and get response synchronously
        CURL* curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("Failed to initialize CURL");
        }
        
        std::string url = sse_url_ + sse_endpoint_;
        std::string request_str = request.dump();
        std::string response_data;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_str.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            throw std::runtime_error(std::string("CURL request failed: ") + curl_easy_strerror(res));
        }
        
        if (http_code != 200) {
            std::cerr << "HTTP error " << http_code << ": " << response_data << std::endl;
            throw std::runtime_error("HTTP request failed with code " + std::to_string(http_code));
        }
        
        if (!response_data.empty()) {
            return json::parse(response_data);
        }
        
        return json::object();
    }
    
    return json::object();
}

void MCPClient::write_request(const json& request) {
    std::string request_str = request.dump();
    
    if (transport_type_ == TransportType::STDIO) {
        request_str += "\n";
        ssize_t written = write(stdin_fd_, request_str.c_str(), request_str.length());
        if (written < 0) {
            throw std::runtime_error("Failed to write to stdin");
        }
        
    } else if (transport_type_ == TransportType::SSE) {
        // For SSE mode, we don't just write - we need to POST and get response in one call
        // This will be handled differently in send_request
    }
}

json MCPClient::read_response() {
    if (transport_type_ == TransportType::STDIO) {
        std::string line;
        char c;
        while (read(stdout_fd_, &c, 1) > 0) {
            if (c == '\n') break;
            line += c;
        }
        
        if (!line.empty()) {
            return json::parse(line);
        }
        
    } else if (transport_type_ == TransportType::SSE) {
        // For SSE, response is handled in send_request directly
        // This shouldn't be called separately
    }
    
    return json::object();
}

} // namespace mcp
