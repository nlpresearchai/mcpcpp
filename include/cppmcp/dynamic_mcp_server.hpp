#pragma once

#include "mcp_server.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

using json = nlohmann::json;
using mcp::MCPServer;
using mcp::Tool;

namespace dynamic_mcp {

// ==================== CONFIGURATION STRUCTURES ====================

struct TaskParameter {
    std::string name;
    std::string type;
    bool required = true;
    std::string description;
    json default_value;
};

struct TaskConfig {
    std::string name;
    std::string description;
    std::string operation_type;  // database, rest_api, terminal, file_operation, data_processing
    json config;
    std::vector<TaskParameter> parameters;
};

struct WorkflowStep {
    std::string name;
    std::string task;
    std::vector<std::string> dependencies;
    std::map<std::string, std::string> input_mapping;
    std::map<std::string, std::string> output_mapping;
};

struct WorkflowConfig {
    std::string name;
    std::string description;
    std::vector<TaskParameter> parameters;
    std::vector<WorkflowStep> steps;
};

// ==================== TASK EXECUTORS ====================

class TaskExecutor {
public:
    virtual ~TaskExecutor() = default;
    virtual json execute(const json& task_config, const json& params) = 0;
};

class DatabaseExecutor : public TaskExecutor {
public:
    json execute(const json& task_config, const json& params) override;
};

class RestApiExecutor : public TaskExecutor {
public:
    json execute(const json& task_config, const json& params) override;
};

class TerminalExecutor : public TaskExecutor {
public:
    json execute(const json& task_config, const json& params) override;
};

class FileOperationExecutor : public TaskExecutor {
public:
    json execute(const json& task_config, const json& params) override;
};

class DataProcessingExecutor : public TaskExecutor {
public:
    json execute(const json& task_config, const json& params) override;
};

// ==================== CONFIGURATION LOADER ====================

class ConfigLoader {
public:
    explicit ConfigLoader(const std::string& config_path);
    
    bool load();
    
    json get_server_info() const { return server_info_; }
    const std::vector<TaskConfig>& get_tasks() const { return tasks_; }
    const std::vector<WorkflowConfig>& get_workflows() const { return workflows_; }
    
private:
    std::string config_path_;
    json server_info_;
    std::vector<TaskConfig> tasks_;
    std::vector<WorkflowConfig> workflows_;
    
    TaskConfig parse_task(const json& task_json);
    WorkflowConfig parse_workflow(const json& workflow_json);
    TaskParameter parse_parameter(const json& param_json);
};

// ==================== WORKFLOW EXECUTOR ====================

class WorkflowExecutor {
public:
    using TaskRegistry = std::map<std::string, std::function<json(const json&)>>;
    
    explicit WorkflowExecutor(const TaskRegistry& registry);
    
    json execute(const WorkflowConfig& workflow, const json& params);
    
private:
    const TaskRegistry& task_registry_;
    
    std::vector<std::string> resolve_dependencies(const std::vector<WorkflowStep>& steps);
    json replace_variables(const json& value, const std::map<std::string, json>& variables);
};

// ==================== DYNAMIC TOOL GENERATOR ====================

class DynamicToolGenerator {
public:
    explicit DynamicToolGenerator(const ConfigLoader& config_loader);
    
    void generate_all_tools(MCPServer& server);
    
private:
    const ConfigLoader& config_loader_;
    std::map<std::string, std::unique_ptr<TaskExecutor>> executors_;
    std::map<std::string, std::function<json(const json&)>> task_registry_;
    
    void create_task_tool(MCPServer& server, const TaskConfig& task);
    void create_workflow_tool(MCPServer& server, const WorkflowConfig& workflow);
    
    // Helper to replace placeholders in strings
    std::string replace_placeholders(const std::string& text, const json& params);
    json replace_placeholders_in_json(const json& obj, const json& params);
};

// ==================== UTILITY FUNCTIONS ====================

// Convert JSON type string to actual type check
bool validate_parameter_type(const std::string& type_str, const json& value);

// Create error response
json create_error_response(const std::string& error_message);

// Create success response
json create_success_response(const json& data);

} // namespace dynamic_mcp
