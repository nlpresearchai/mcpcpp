# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2025-12-28

### Added
- Initial release of cppmcp library
- Full MCP protocol support (v2024-11-05)
- Server implementation with STDIO and SSE transport
- Client implementation for connecting to MCP servers
- Dynamic configuration system for JSON-based server setup
- Examples for common use cases
- Comprehensive documentation
- CMake build system with FetchContent for dependencies
- MIT License

### Features
- Tools: Register C++ functions as AI-callable tools
- Resources: Serve data and configurations
- Prompts: Provide reusable prompt templates
- JSON-RPC 2.0 message handling
- HTTP/SSE server for remote connections
- STDIO transport for local process communication

## [Unreleased]

### Planned
- WebSocket transport support
- Authentication and authorization
- Rate limiting
- Metrics and monitoring
- Python bindings
- More examples and tutorials
