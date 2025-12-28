#!/bin/bash
# Build script for cppmcp library

set -e

echo "=========================================="
echo "Building cppmcp Library"
echo "=========================================="

# Create build directory
mkdir -p build
cd build

# Configure
echo "Configuring..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCPPMCP_BUILD_EXAMPLES=OFF \
    -DCPPMCP_BUILD_TESTS=OFF

# Build
echo "Building..."
cmake --build . -j$(nproc)

# Summary
echo ""
echo "=========================================="
echo "Build Complete!"
echo "=========================================="
echo ""
ls -lh libcppmcp.* 2>/dev/null || true
echo ""
echo "Library files created successfully!"
echo ""
echo "To use the library:"
echo "  1. Include: #include <cppmcp/mcp_server.hpp>"
echo "  2. Link: -lcppmcp or target_link_libraries(your_target cppmcp)"
echo "  3. Add include path: -I../include"
echo ""
