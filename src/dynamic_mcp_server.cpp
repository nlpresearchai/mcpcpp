#include <cppmcp/dynamic_mcp_server.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <array>
#include <memory>
#include <regex>
#include <set>
#include <curl/curl.h>

namespace dynamic_mcp {

// ==================== UTILITY FUNCTIONS ====================

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

bool validate_parameter_type(const std::string& type_str, const json& value) {
    if (type_str == "string" || type_str == "str") {
        return value.is_string();
    } else if (type_str == "integer" || type_str == "int") {
        return value.is_number_integer();
    } else if (type_str == "float" || type_str == "double" || type_str == "number") {
        return value.is_number();
    } else if (type_str == "boolean" || type_str == "bool") {
        return value.is_boolean();
    } else if (type_str == "object") {
        return value.is_object();
    } else if (type_str == "array") {
        return value.is_array();
    }
    return true; // Allow any type if not specified
}

json create_error_response(const std::string& error_message) {
    return {
        {"success", false},
        {"error", error_message}
    };
}

json create_success_response(const json& data) {
    json response = {{"success", true}};
    if (!data.is_null()) {
        response["data"] = data;
    }
    return response;
}

// ==================== DATABASE EXECUTOR ====================

json DatabaseExecutor::execute(const json& task_config, const json& params) {
    try {
        std::string db_type = task_config.value("db_type", "postgresql");
        std::string query = task_config.value("query", "");
        std::string connection_string = task_config.value("connection_string", "");
        
        // Replace parameters in query
        for (auto& [key, value] : params.items()) {
            std::string placeholder = "{" + key + "}";
            std::string replacement;
            
            if (value.is_string()) {
                std::string str_val = value.get<std::string>();
                // Escape single quotes for SQL
                std::string escaped;
                for (char c : str_val) {
                    if (c == '\'') escaped += "''";
                    else escaped += c;
                }
                replacement = "'" + escaped + "'";
            } else if (value.is_number()) {
                replacement = value.dump();
            } else {
                replacement = value.dump();
            }
            
            size_t pos = 0;
            while ((pos = query.find(placeholder, pos)) != std::string::npos) {
                query.replace(pos, placeholder.length(), replacement);
                pos += replacement.length();
            }
        }
        
        std::cerr << "📊 Database " << db_type << " query: " << query << std::endl;
        
        // NOTE: This is a mock implementation
        // In production, use proper database drivers (libpq for PostgreSQL, etc.)
        return {
            {"success", true},
            {"message", "Database operation simulated (would execute: " + query + ")"},
            {"db_type", db_type},
            {"query", query},
            {"note", "Install database drivers for real operations"}
        };
        
    } catch (const std::exception& e) {
        return create_error_response(std::string("Database error: ") + e.what());
    }
}

// ==================== REST API EXECUTOR ====================

json RestApiExecutor::execute(const json& task_config, const json& params) {
    try {
        std::string method = task_config.value("method", "GET");
        std::string url = task_config.value("url", "");
        json headers = task_config.value("headers", json::object());
        json query_params = task_config.value("query_params", json::object());
        json body = task_config.value("body", json::object());
        
        // Replace placeholders in all fields
        auto replace_in_string = [&params](const std::string& str) {
            std::string result = str;
            for (auto& [key, value] : params.items()) {
                std::string placeholder = "{" + key + "}";
                std::string replacement = value.is_string() ? value.get<std::string>() : value.dump();
                size_t pos = 0;
                while ((pos = result.find(placeholder, pos)) != std::string::npos) {
                    result.replace(pos, placeholder.length(), replacement);
                    pos += replacement.length();
                }
            }
            return result;
        };
        
        std::function<json(const json&)> replace_in_json = [&](const json& obj) -> json {
            if (obj.is_string()) {
                return replace_in_string(obj.get<std::string>());
            } else if (obj.is_object()) {
                json result = json::object();
                for (auto& [k, v] : obj.items()) {
                    result[k] = replace_in_json(v);
                }
                return result;
            } else if (obj.is_array()) {
                json result = json::array();
                for (auto& item : obj) {
                    result.push_back(replace_in_json(item));
                }
                return result;
            }
            return obj;
        };
        
        url = replace_in_string(url);
        headers = replace_in_json(headers);
        query_params = replace_in_json(query_params);
        body = replace_in_json(body);
        
        // Build URL with query parameters
        if (!query_params.empty()) {
            url += "?";
            bool first = true;
            for (auto& [key, value] : query_params.items()) {
                if (!first) url += "&";
                url += key + "=" + (value.is_string() ? value.get<std::string>() : value.dump());
                first = false;
            }
        }
        
        std::cerr << "🌐 REST API: " << method << " " << url << std::endl;
        
        // Execute request using libcurl
        CURL* curl = curl_easy_init();
        if (!curl) {
            return create_error_response("Failed to initialize CURL");
        }
        
        std::string response_data;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        
        // Set headers
        struct curl_slist* curl_headers = nullptr;
        for (auto& [key, value] : headers.items()) {
            std::string header = key + ": " + (value.is_string() ? value.get<std::string>() : value.dump());
            curl_headers = curl_slist_append(curl_headers, header.c_str());
        }
        if (curl_headers) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
        }
        
        // Set method and body
        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            if (!body.empty()) {
                std::string body_str = body.dump();
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
            }
        } else if (method == "PUT") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            if (!body.empty()) {
                std::string body_str = body.dump();
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
            }
        } else if (method == "DELETE") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        }
        
        CURLcode res = curl_easy_perform(curl);
        long status_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
        
        if (curl_headers) {
            curl_slist_free_all(curl_headers);
        }
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            return create_error_response(std::string("CURL error: ") + curl_easy_strerror(res));
        }
        
        // Try to parse response as JSON
        json response_json;
        try {
            response_json = json::parse(response_data);
        } catch (...) {
            response_json = response_data;
        }
        
        return {
            {"success", true},
            {"status_code", status_code},
            {"data", response_json},
            {"method", method},
            {"url", url}
        };
        
    } catch (const std::exception& e) {
        return create_error_response(std::string("REST API error: ") + e.what());
    }
}

// ==================== TERMINAL EXECUTOR ====================

json TerminalExecutor::execute(const json& task_config, const json& params) {
    try {
        std::string command = task_config.value("command", "");
        int timeout = task_config.value("timeout", 30);
        
        // Replace parameters in command
        for (auto& [key, value] : params.items()) {
            std::string placeholder = "{" + key + "}";
            std::string replacement = value.is_string() ? value.get<std::string>() : value.dump();
            size_t pos = 0;
            while ((pos = command.find(placeholder, pos)) != std::string::npos) {
                command.replace(pos, placeholder.length(), replacement);
                pos += replacement.length();
            }
        }
        
        std::cerr << "💻 Executing: " << command << std::endl;
        
        // Execute command and capture output
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
        
        if (!pipe) {
            return create_error_response("Failed to execute command");
        }
        
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        
        int returncode = pclose(pipe.release()) / 256;
        
        return {
            {"success", returncode == 0},
            {"returncode", returncode},
            {"stdout", result},
            {"stderr", ""},
            {"command", command}
        };
        
    } catch (const std::exception& e) {
        return create_error_response(std::string("Terminal error: ") + e.what());
    }
}

// ==================== FILE OPERATION EXECUTOR ====================

json FileOperationExecutor::execute(const json& task_config, const json& params) {
    try {
        std::string action = task_config.value("action", "read");
        std::string encoding = task_config.value("encoding", "utf-8");
        bool create_dirs = task_config.value("create_dirs", false);
        
        if (!params.contains("file_path")) {
            return create_error_response("file_path is required");
        }
        
        std::string file_path = params["file_path"];
        
        if (action == "read") {
            std::ifstream file(file_path);
            if (!file.is_open()) {
                return create_error_response("File not found: " + file_path);
            }
            
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();
            
            return {
                {"success", true},
                {"content", content},
                {"file_path", file_path},
                {"size", content.size()}
            };
        }
        else if (action == "write") {
            if (!params.contains("content")) {
                return create_error_response("content is required for write operation");
            }
            
            std::string content = params["content"];
            
            // TODO: Create directories if needed
            
            std::ofstream file(file_path);
            if (!file.is_open()) {
                return create_error_response("Failed to open file for writing: " + file_path);
            }
            
            file << content;
            file.close();
            
            return {
                {"success", true},
                {"message", "Written " + std::to_string(content.size()) + " characters to " + file_path},
                {"file_path", file_path}
            };
        }
        else if (action == "append") {
            if (!params.contains("content")) {
                return create_error_response("content is required for append operation");
            }
            
            std::string content = params["content"];
            
            std::ofstream file(file_path, std::ios::app);
            if (!file.is_open()) {
                return create_error_response("Failed to open file for appending: " + file_path);
            }
            
            file << content;
            file.close();
            
            return {
                {"success", true},
                {"message", "Appended " + std::to_string(content.size()) + " characters to " + file_path},
                {"file_path", file_path}
            };
        }
        
        return create_error_response("Unknown action: " + action);
        
    } catch (const std::exception& e) {
        return create_error_response(std::string("File operation error: ") + e.what());
    }
}

// ==================== DATA PROCESSING EXECUTOR ====================

json DataProcessingExecutor::execute(const json& task_config, const json& params) {
    try {
        std::string processor = task_config.value("processor", "json_parser");
        
        if (processor == "json_parser") {
            if (!params.contains("json_string")) {
                return create_error_response("json_string is required");
            }
            
            std::string json_string = params["json_string"];
            json parsed = json::parse(json_string);
            
            return {
                {"success", true},
                {"data", parsed},
                {"processor", processor}
            };
        }
        else if (processor == "csv_transformer") {
            if (!params.contains("csv_data")) {
                return create_error_response("csv_data is required");
            }
            
            std::string csv_data = params["csv_data"];
            std::string operation = params.value("operation", "parse");
            std::string delimiter = task_config.value("delimiter", ",");
            
            // Simple CSV parsing
            std::vector<std::vector<std::string>> rows;
            std::istringstream stream(csv_data);
            std::string line;
            
            while (std::getline(stream, line)) {
                std::vector<std::string> row;
                std::istringstream line_stream(line);
                std::string cell;
                
                while (std::getline(line_stream, cell, delimiter[0])) {
                    row.push_back(cell);
                }
                
                if (!row.empty()) {
                    rows.push_back(row);
                }
            }
            
            return {
                {"success", true},
                {"rows", rows},
                {"row_count", rows.size()},
                {"processor", processor},
                {"operation", operation}
            };
        }
        
        return create_error_response("Unknown processor: " + processor);
        
    } catch (const json::parse_error& e) {
        return create_error_response(std::string("JSON parse error: ") + e.what());
    } catch (const std::exception& e) {
        return create_error_response(std::string("Data processing error: ") + e.what());
    }
}

// ==================== CONFIG LOADER ====================

ConfigLoader::ConfigLoader(const std::string& config_path)
    : config_path_(config_path) {}

bool ConfigLoader::load() {
    try {
        std::ifstream file(config_path_);
        if (!file.is_open()) {
            std::cerr << "❌ Failed to open config file: " << config_path_ << std::endl;
            return false;
        }
        
        json config;
        file >> config;
        
        // Load server info
        server_info_ = config.value("server_info", json::object({
            {"name", "DynamicTaskServer"},
            {"version", "1.0.0"},
            {"description", "Dynamic MCP server"}
        }));
        
        // Load tasks
        if (config.contains("tasks") && config["tasks"].is_array()) {
            for (const auto& task_json : config["tasks"]) {
                tasks_.push_back(parse_task(task_json));
            }
        }
        
        // Load workflows
        if (config.contains("workflows") && config["workflows"].is_array()) {
            for (const auto& workflow_json : config["workflows"]) {
                workflows_.push_back(parse_workflow(workflow_json));
            }
        }
        
        std::cerr << "✅ Loaded " << tasks_.size() << " tasks and " 
                  << workflows_.size() << " workflows" << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to load config: " << e.what() << std::endl;
        return false;
    }
}

TaskConfig ConfigLoader::parse_task(const json& task_json) {
    TaskConfig task;
    
    if (task_json.contains("name") && task_json["name"].is_string()) {
        task.name = task_json["name"];
    }
    
    if (task_json.contains("description") && task_json["description"].is_string()) {
        task.description = task_json["description"];
    }
    
    if (task_json.contains("operation_type") && task_json["operation_type"].is_string()) {
        task.operation_type = task_json["operation_type"];
    }
    
    if (task_json.contains("config")) {
        task.config = task_json["config"];
    }
    
    if (task_json.contains("parameters") && task_json["parameters"].is_array()) {
        for (const auto& param_json : task_json["parameters"]) {
            task.parameters.push_back(parse_parameter(param_json));
        }
    }
    
    return task;
}

WorkflowConfig ConfigLoader::parse_workflow(const json& workflow_json) {
    WorkflowConfig workflow;
    
    if (workflow_json.contains("name") && workflow_json["name"].is_string()) {
        workflow.name = workflow_json["name"];
    }
    
    if (workflow_json.contains("description") && workflow_json["description"].is_string()) {
        workflow.description = workflow_json["description"];
    }
    
    if (workflow_json.contains("parameters") && workflow_json["parameters"].is_array()) {
        for (const auto& param_json : workflow_json["parameters"]) {
            workflow.parameters.push_back(parse_parameter(param_json));
        }
    }
    
    if (workflow_json.contains("steps") && workflow_json["steps"].is_array()) {
        for (const auto& step_json : workflow_json["steps"]) {
            WorkflowStep step;
            
            if (step_json.contains("name") && step_json["name"].is_string()) {
                step.name = step_json["name"];
            }
            
            if (step_json.contains("task") && step_json["task"].is_string()) {
                step.task = step_json["task"];
            }
            
            if (step_json.contains("dependencies") && step_json["dependencies"].is_array()) {
                for (const auto& dep : step_json["dependencies"]) {
                    if (dep.is_string()) {
                        step.dependencies.push_back(dep);
                    }
                }
            }
            
            if (step_json.contains("input_mapping") && step_json["input_mapping"].is_object()) {
                for (auto& [key, value] : step_json["input_mapping"].items()) {
                    if (value.is_string()) {
                        step.input_mapping[key] = value;
                    }
                }
            }
            
            if (step_json.contains("output_mapping") && step_json["output_mapping"].is_object()) {
                for (auto& [key, value] : step_json["output_mapping"].items()) {
                    if (value.is_string()) {
                        step.output_mapping[key] = value;
                    }
                }
            }
            
            workflow.steps.push_back(step);
        }
    }
    
    return workflow;
}

TaskParameter ConfigLoader::parse_parameter(const json& param_json) {
    TaskParameter param;
    
    // Safely extract values with proper type handling
    if (param_json.contains("name")) {
        param.name = param_json["name"].is_string() ? param_json["name"].get<std::string>() : "";
    }
    
    if (param_json.contains("type")) {
        param.type = param_json["type"].is_string() ? param_json["type"].get<std::string>() : "string";
    } else {
        param.type = "string";
    }
    
    if (param_json.contains("required")) {
        param.required = param_json["required"].is_boolean() ? param_json["required"].get<bool>() : true;
    } else {
        param.required = true;
    }
    
    if (param_json.contains("description")) {
        param.description = param_json["description"].is_string() ? param_json["description"].get<std::string>() : "";
    }
    
    if (param_json.contains("default")) {
        param.default_value = param_json["default"];
    }
    
    return param;
}

// ==================== WORKFLOW EXECUTOR ====================

WorkflowExecutor::WorkflowExecutor(const TaskRegistry& registry)
    : task_registry_(registry) {}

json WorkflowExecutor::execute(const WorkflowConfig& workflow, const json& params) {
    try {
        std::map<std::string, json> step_results;
        auto execution_order = resolve_dependencies(workflow.steps);
        
        std::cerr << "🔄 Executing workflow: " << workflow.name << std::endl;
        
        for (const auto& step_name : execution_order) {
            // Find the step
            auto step_it = std::find_if(workflow.steps.begin(), workflow.steps.end(),
                [&step_name](const WorkflowStep& s) { return s.name == step_name; });
            
            if (step_it == workflow.steps.end()) {
                return create_error_response("Step not found: " + step_name);
            }
            
            const WorkflowStep& step = *step_it;
            
            // Prepare step parameters
            json step_params = params;
            
            // Apply input mapping
            for (const auto& [param_name, mapping_value] : step.input_mapping) {
                json mapped_value = replace_variables(mapping_value, step_results);
                step_params[param_name] = mapped_value;
            }
            
            // Execute the task
            auto task_it = task_registry_.find(step.task);
            if (task_it == task_registry_.end()) {
                return create_error_response("Task not found: " + step.task);
            }
            
            std::cerr << "  ▶ Executing step: " << step_name << " (task: " << step.task << ")" << std::endl;
            json result = task_it->second(step_params);
            
            // Store results with output mapping
            for (const auto& [result_key, mapped_name] : step.output_mapping) {
                if (result.contains(result_key)) {
                    step_results[mapped_name] = result[result_key];
                }
            }
            
            // Store the full result
            step_results[step_name] = result;
            
            // Check for failure
            if (result.contains("success") && !result["success"].get<bool>()) {
                return {
                    {"success", false},
                    {"failed_step", step_name},
                    {"error", result.value("error", "Unknown error")},
                    {"step_results", step_results}
                };
            }
        }
        
        return {
            {"success", true},
            {"workflow", workflow.name},
            {"steps_executed", execution_order.size()},
            {"step_results", step_results}
        };
        
    } catch (const std::exception& e) {
        return create_error_response(std::string("Workflow error: ") + e.what());
    }
}

std::vector<std::string> WorkflowExecutor::resolve_dependencies(const std::vector<WorkflowStep>& steps) {
    std::map<std::string, WorkflowStep> step_map;
    for (const auto& step : steps) {
        step_map[step.name] = step;
    }
    
    std::set<std::string> visited;
    std::vector<std::string> order;
    
    std::function<void(const std::string&)> visit = [&](const std::string& step_name) {
        if (visited.count(step_name)) return;
        visited.insert(step_name);
        
        const auto& step = step_map[step_name];
        for (const auto& dep : step.dependencies) {
            if (step_map.count(dep)) {
                visit(dep);
            }
        }
        
        order.push_back(step_name);
    };
    
    for (const auto& step : steps) {
        visit(step.name);
    }
    
    return order;
}

json WorkflowExecutor::replace_variables(const json& value, const std::map<std::string, json>& variables) {
    if (value.is_string()) {
        std::string str = value.get<std::string>();
        
        // Replace {variable} patterns
        for (const auto& [var_name, var_value] : variables) {
            std::string placeholder = "{" + var_name + "}";
            std::string replacement = var_value.is_string() ? var_value.get<std::string>() : var_value.dump();
            
            size_t pos = 0;
            while ((pos = str.find(placeholder, pos)) != std::string::npos) {
                str.replace(pos, placeholder.length(), replacement);
                pos += replacement.length();
            }
        }
        
        return str;
    }
    
    return value;
}

// ==================== DYNAMIC TOOL GENERATOR ====================

DynamicToolGenerator::DynamicToolGenerator(const ConfigLoader& config_loader)
    : config_loader_(config_loader) {
    
    // Initialize executors
    executors_["database"] = std::make_unique<DatabaseExecutor>();
    executors_["rest_api"] = std::make_unique<RestApiExecutor>();
    executors_["terminal"] = std::make_unique<TerminalExecutor>();
    executors_["file_operation"] = std::make_unique<FileOperationExecutor>();
    executors_["data_processing"] = std::make_unique<DataProcessingExecutor>();
}

void DynamicToolGenerator::generate_all_tools(MCPServer& server) {
    // Generate task tools
    for (const auto& task : config_loader_.get_tasks()) {
        create_task_tool(server, task);
    }
    
    // Generate workflow tools
    for (const auto& workflow : config_loader_.get_workflows()) {
        create_workflow_tool(server, workflow);
    }
}

void DynamicToolGenerator::create_task_tool(MCPServer& server, const TaskConfig& task) {
    // Create tool handler
    auto handler = [this, task](const json& arguments) -> json {
        std::cerr << "🔧 Executing task: " << task.name << std::endl;
        
        // Validate and prepare parameters
        json params = arguments;
        
        // Apply defaults for missing parameters
        for (const auto& param : task.parameters) {
            if (!params.contains(param.name)) {
                if (!param.default_value.is_null()) {
                    params[param.name] = param.default_value;
                } else if (param.required) {
                    return create_error_response("Missing required parameter: " + param.name);
                }
            }
        }
        
        // Get executor
        auto exec_it = executors_.find(task.operation_type);
        if (exec_it == executors_.end()) {
            return create_error_response("Unknown operation type: " + task.operation_type);
        }
        
        // Execute
        return exec_it->second->execute(task.config, params);
    };
    
    // Store in registry for workflows
    task_registry_[task.name] = handler;
    
    // Build input schema
    json properties = json::object();
    json required = json::array();
    
    for (const auto& param : task.parameters) {
        json param_schema = {
            {"description", param.description}
        };
        
        // Map type
        if (param.type == "integer" || param.type == "int") {
            param_schema["type"] = "number";
        } else if (param.type == "float" || param.type == "double" || param.type == "number") {
            param_schema["type"] = "number";
        } else if (param.type == "boolean" || param.type == "bool") {
            param_schema["type"] = "boolean";
        } else if (param.type == "object") {
            param_schema["type"] = "object";
        } else if (param.type == "array") {
            param_schema["type"] = "array";
        } else {
            param_schema["type"] = "string";
        }
        
        properties[param.name] = param_schema;
        
        if (param.required && param.default_value.is_null()) {
            required.push_back(param.name);
        }
    }
    
    json input_schema = {
        {"type", "object"},
        {"properties", properties}
    };
    
    if (!required.empty()) {
        input_schema["required"] = required;
    }
    
    // Register tool
    server.add_tool(
        task.name,
        task.description + " [Operation: " + task.operation_type + "]",
        input_schema,
        handler
    );
    
    std::cerr << "  ✓ Registered task: " << task.name << " (" << task.operation_type << ")" << std::endl;
}

void DynamicToolGenerator::create_workflow_tool(MCPServer& server, const WorkflowConfig& workflow) {
    // Create workflow executor
    auto workflow_executor = std::make_shared<WorkflowExecutor>(task_registry_);
    
    // Create tool handler
    auto handler = [this, workflow, workflow_executor](const json& arguments) -> json {
        std::cerr << "🔄 Executing workflow: " << workflow.name << std::endl;
        return workflow_executor->execute(workflow, arguments);
    };
    
    // Build input schema
    json properties = json::object();
    json required = json::array();
    
    for (const auto& param : workflow.parameters) {
        json param_schema = {
            {"description", param.description}
        };
        
        // Map type (same as task parameters)
        if (param.type == "integer" || param.type == "int") {
            param_schema["type"] = "number";
        } else if (param.type == "float" || param.type == "double" || param.type == "number") {
            param_schema["type"] = "number";
        } else if (param.type == "boolean" || param.type == "bool") {
            param_schema["type"] = "boolean";
        } else {
            param_schema["type"] = "string";
        }
        
        properties[param.name] = param_schema;
        
        if (param.required && param.default_value.is_null()) {
            required.push_back(param.name);
        }
    }
    
    json input_schema = {
        {"type", "object"},
        {"properties", properties}
    };
    
    if (!required.empty()) {
        input_schema["required"] = required;
    }
    
    // Register tool
    server.add_tool(
        workflow.name,
        workflow.description + " [Workflow with " + std::to_string(workflow.steps.size()) + " steps]",
        input_schema,
        handler
    );
    
    std::cerr << "  ✓ Registered workflow: " << workflow.name << std::endl;
}

} // namespace dynamic_mcp
