# mcpcpp - C++ Model Context Protocol Library

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/CMake-3.14+-green.svg)](https://cmake.org/)

A modern C++ implementation of the [Model Context Protocol (MCP)](https://modelcontextprotocol.io/) for building AI-powered applications with external tool integration.

## 🚀 Features

- ✅ **Full MCP Protocol Support** - JSON-RPC 2.0 based protocol (v2024-11-05)
- ✅ **Dual Transport** - STDIO and SSE/HTTP modes
- ✅ **Tools** - Register C++ functions as AI-callable tools
- ✅ **Resources** - Serve data, configurations, and files
- ✅ **Prompts** - Provide reusable prompt templates
- ✅ **Client Library** - Connect to MCP servers from C++
- ✅ **Dynamic Configuration** - Load tools from JSON config files
- ✅ **Header-Only Option** - Easy integration
- ✅ **Modern C++17** - Type-safe and efficient

## 📦 Quick Start

### Installation

```bash
git clone https://github.com/nlpresearchai/mcpcpp.git
cd mcpcpp
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
sudo cmake --install .
```

### Basic Usage

```cpp
#include <cppmcp/mcp_server.hpp>

int main() {
    // Create server
    mcp::MCPServer server("my-server", "1.0.0");
    
    // Add a tool
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
            return {{"result", args["a"].get<double>() + args["b"].get<double>()}};
        }
    );
    
    // Run server (STDIO mode)
    server.run_stdio();
    
    return 0;
}
```

### Using in Your Project

**CMakeLists.txt:**
```cmake
find_package(cppmcp REQUIRED)

add_executable(my_server main.cpp)
target_link_libraries(my_server PRIVATE cppmcp::cppmcp)
```

## 📚 Documentation

### Core Components

#### 1. **MCP Server** (`mcp_server.hpp`)

Create servers that expose tools, resources, and prompts to AI applications.

```cpp
mcp::MCPServer server("server-name", "1.0.0");

// Add tool
server.add_tool("tool_name", "description", input_schema, 
    [](const json& args) { return result; });

// Add resource
server.add_resource("uri://resource", "name", "description", "mime-type",
    []() { return "data"; });

// Add prompt
server.add_prompt("prompt_name", "description", arguments,
    [](const json& args) { return prompt; });

// Run
server.run_stdio();  // or server.run_sse(port);
```

#### 2. **MCP Client** (`mcp_client.hpp`)

Connect to MCP servers and call tools.

```cpp
mcp::MCPClient client("client-name", "1.0.0");

// Connect
client.connect_sse("http://localhost:8080");
client.initialize();

// List and call tools
auto tools = client.list_tools();
auto result = client.call_tool("add", {{"a", 5}, {"b", 3}});

// Read resources
auto resources = client.list_resources();
auto data = client.read_resource("config://app");
```

#### 3. **Dynamic Server** (`dynamic_mcp_server.hpp`)

Load server configuration from JSON files.

```cpp
#include <cppmcp/dynamic_mcp_server.hpp>

dynamic_mcp::ConfigLoader loader("tasks_config.json");
loader.load();

// Server is automatically configured from JSON
```

## 📖 Examples

See the [`examples/`](examples/) directory:

- **`simple_server.cpp`** - Minimal MCP server
- **`server_with_tools.cpp`** - Server with multiple tools, resources, and prompts
- **`dynamic_server.cpp`** - Configuration-driven server
- **`client_example.cpp`** - MCP client usage

### Building Examples

```bash
cd build
cmake .. -DCPPMCP_BUILD_EXAMPLES=ON
cmake --build .
./examples/simple_server stdio
./examples/server_with_tools sse 8080
```

## 🏗️ Project Structure

```
mcpcpp/
├── include/cppmcp/          # Public headers
│   ├── mcp_server.hpp       # Server API
│   ├── mcp_client.hpp       # Client API
│   └── dynamic_mcp_server.hpp  # Dynamic configuration
├── src/                     # Implementation
│   ├── mcp_server.cpp
│   ├── mcp_sse.cpp
│   ├── mcp_client.cpp
│   └── dynamic_mcp_server.cpp
├── examples/                # Usage examples
├── tests/                   # Unit tests
├── docs/                    # Documentation
└── CMakeLists.txt          # Build configuration
```

## 🛠️ Building

### Requirements

- **C++17** compiler (GCC 7+, Clang 5+, MSVC 2017+)
- **CMake** 3.14+
- **libcurl** (for HTTP/SSE support)

Dependencies (auto-fetched by CMake):
- [nlohmann/json](https://github.com/nlohmann/json) - JSON parsing
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) - HTTP server

### CMake Options

```cmake
-DCPPMCP_BUILD_EXAMPLES=ON   # Build examples (default: ON)
-DCPPMCP_BUILD_TESTS=ON      # Build tests (default: ON)
-DCPPMCP_BUILD_SHARED=ON     # Build shared library (default: ON)
-DCPPMCP_BUILD_STATIC=ON     # Build static library (default: ON)
```

### Ubuntu/Debian

```bash
sudo apt-get install build-essential cmake libcurl4-openssl-dev
```

### macOS

```bash
brew install cmake curl
```

## 🧪 Testing

```bash
cd build
cmake .. -DCPPMCP_BUILD_TESTS=ON
cmake --build .
ctest
```

## 🔌 Integration with AI Clients

### Claude Desktop

Add to `~/Library/Application Support/Claude/claude_desktop_config.json`:

```json
{
  "mcpServers": {
    "cppmcp-example": {
      "command": "/path/to/build/examples/simple_server",
      "args": ["stdio"]
    }
  }
}
```

### Custom Python Client

```python
import httpx
import json

# Connect to SSE server
response = httpx.post(
    "http://localhost:8080/message",
    json={
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {
            "protocolVersion": "2024-11-05",
            "capabilities": {},
            "clientInfo": {"name": "python-client", "version": "1.0"}
        }
    }
)
print(response.json())
```

## 📊 Performance

- **Low Latency** - Direct C++ execution, no Python overhead
- **High Throughput** - Handles thousands of requests per second
- **Memory Efficient** - Minimal allocations, optimized data structures
- **Async Ready** - Non-blocking SSE connections

## 🤝 Contributing

Contributions welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🔗 Related Projects

- [Model Context Protocol](https://modelcontextprotocol.io/) - Official MCP specification
- [mcp Python SDK](https://github.com/modelcontextprotocol/python-sdk) - Official Python implementation
- [mcp TypeScript SDK](https://github.com/modelcontextprotocol/typescript-sdk) - Official TypeScript implementation

## 📬 Contact

- **Issues**: [GitHub Issues](https://github.com/nlpresearchai/mcpcpp/issues)
- **Discussions**: [GitHub Discussions](https://github.com/nlpresearchai/mcpcpp/discussions)

## ⭐ Star History

If you find this project helpful, please consider giving it a star!

---

**Built with ❤️ for the AI and C++ communities**

# mcpcpp
