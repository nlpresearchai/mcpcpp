#include <cppmcp/mcp_server.hpp>
#include <httplib.h>
#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace mcp {

// SSE connection state
struct SSEConnection {
    std::queue<std::string> message_queue;
    std::mutex mutex;
    std::condition_variable cv;
    std::atomic<bool> active{true};
};

void MCPServer::run_sse_server(int port) {
    std::cerr << "MCP Server '" << server_name_ << "' starting in SSE mode on port " << port << "..." << std::endl;
    std::cerr << "Using Streamable HTTP transport (MCP 2024-11-05+)" << std::endl;
    
    httplib::Server server;
    
    // Store active SSE connections per session
    std::map<std::string, std::shared_ptr<SSEConnection>> connections;
    std::mutex connections_mutex;
    
    // Store session state
    std::map<std::string, bool> sessions;
    std::mutex sessions_mutex;
    
    // Connection cleanup helper
    auto cleanup_stale_connections = [&connections, &connections_mutex]() {
        std::lock_guard<std::mutex> lock(connections_mutex);
        // Remove inactive connections
        for (auto it = connections.begin(); it != connections.end(); ) {
            if (!it->second->active) {
                std::cerr << "Cleaning up stale connection: " << it->first << std::endl;
                it = connections.erase(it);
            } else {
                ++it;
            }
        }
        std::cerr << "Active connections: " << connections.size() << std::endl;
    };
    
    // Health check endpoint (optional, not part of MCP spec)
    server.Get("/health", [&cleanup_stale_connections](const httplib::Request&, httplib::Response& res) {
        cleanup_stale_connections();  // Clean up on health checks
        res.set_content(R"({"status":"ok"})", "application/json");
    });
    
    // Main MCP endpoint - GET method for SSE stream
    server.Get("/", [&](const httplib::Request& req, httplib::Response& res) {
        // Check Accept header
        auto accept = req.get_header_value("Accept");
        if (accept.find("text/event-stream") == std::string::npos) {
            res.status = 406; // Not Acceptable
            res.set_content(R"({"error":"text/event-stream required in Accept header"})", "application/json");
            return;
        }
        
        // Define connection limit
        const size_t MAX_CONNECTIONS = 20;
        
        // Clean up stale connections before creating new one
        {
            std::lock_guard<std::mutex> lock(connections_mutex);
            size_t before = connections.size();
            
            for (auto it = connections.begin(); it != connections.end(); ) {
                if (!it->second->active) {
                    std::cerr << "Cleaning up inactive connection: " << it->first << std::endl;
                    it = connections.erase(it);
                } else {
                    ++it;
                }
            }
            
            std::cerr << "Cleanup: " << before << " -> " << connections.size() << " connections" << std::endl;
            
            // Check connection limit after cleanup
            if (connections.size() >= MAX_CONNECTIONS) {
                std::cerr << "Connection limit reached: " << connections.size() 
                          << "/" << MAX_CONNECTIONS << std::endl;
                res.status = 503;
                res.set_content("Service Unavailable: Too many connections", "text/plain");
                return;
            }
        }
        
        // Generate connection ID (or use session ID if provided)
        std::string session_id = req.get_header_value("Mcp-Session-Id");
        std::string connection_id;
        
        if (!session_id.empty()) {
            connection_id = session_id;
        } else {
            // Use timestamp + random to avoid collisions
            connection_id = std::to_string(std::hash<std::string>{}(
                req.remote_addr + std::to_string(time(nullptr)) + std::to_string(rand())
            ));
        }
        
        auto conn = std::make_shared<SSEConnection>();
        
        {
            std::lock_guard<std::mutex> lock(connections_mutex);
            connections[connection_id] = conn;
            std::cerr << "SSE client connected (GET): " << connection_id 
                      << " (total: " << connections.size() << ")" << std::endl;
        }
        
        // Set SSE headers
        res.set_header("Content-Type", "text/event-stream");
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("X-Accel-Buffering", "no"); // Disable buffering for nginx
        
        res.set_content_provider(
            "text/event-stream",
            [conn, connection_id, this](size_t offset, httplib::DataSink& sink) {
                // Send initial endpoint event for old HTTP+SSE transport compatibility
                // This tells the client where to POST messages
                if (offset == 0) {
                    std::string endpoint_event = "event: endpoint\ndata: /message\n\n";
                    sink.write(endpoint_event.c_str(), endpoint_event.size());
                    std::cerr << "Sent endpoint event to client: " << connection_id << std::endl;
                }
                
                // Shorter timeout to detect disconnections faster
                int idle_count = 0;
                const int max_idle = 3;  // Max 3 idle periods before checking
                
                while (conn->active) {
                    std::unique_lock<std::mutex> lock(conn->mutex);
                    
                    // Wait for messages or timeout (shorter: 10 seconds)
                    if (conn->cv.wait_for(lock, std::chrono::seconds(10),
                                         [&conn] { return !conn->message_queue.empty() || !conn->active; })) {
                        
                        if (!conn->active) break;
                        
                        idle_count = 0;  // Reset idle counter
                        
                        while (!conn->message_queue.empty()) {
                            std::string message = conn->message_queue.front();
                            conn->message_queue.pop();
                            
                            // Format as SSE
                            std::string sse_message = "data: " + message + "\n\n";
                            if (!sink.write(sse_message.c_str(), sse_message.size())) {
                                std::cerr << "Failed to write to SSE sink, client disconnected: " 
                                          << connection_id << std::endl;
                                conn->active = false;
                                break;
                            }
                        }
                    } else {
                        // Timeout - send keepalive or close if too many idles
                        idle_count++;
                        if (idle_count >= max_idle) {
                            std::cerr << "Connection idle timeout, closing: " << connection_id << std::endl;
                            conn->active = false;
                            break;
                        }
                        
                        std::string keepalive = ":keepalive\n\n";
                        if (!sink.write(keepalive.c_str(), keepalive.size())) {
                            std::cerr << "Keepalive failed, client disconnected: " << connection_id << std::endl;
                            conn->active = false;
                            break;
                        }
                    }
                }
                sink.done();
                std::cerr << "SSE stream ended for: " << connection_id << std::endl;
                return true;
            },
            [conn, connection_id, &connections, &connections_mutex](bool success) {
                conn->active = false;
                conn->cv.notify_all();
                
                {
                    std::lock_guard<std::mutex> lock(connections_mutex);
                    connections.erase(connection_id);
                }
                
                std::cerr << "SSE client disconnected: " << connection_id << std::endl;
            }
        );
    });
    
    // Main MCP endpoint - POST method for requests
    server.Post("/", [&](const httplib::Request& req, httplib::Response& res) {
        // Set CORS headers
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Mcp-Session-Id");
        
        try {
            // Parse incoming JSON-RPC message
            json request = json::parse(req.body);
            
            // Handle the request
            json response = handle_message(request);
            
            // For old HTTP+SSE transport, always return JSON response
            res.set_content(response.dump(), "application/json");
            
            // Also broadcast via SSE if there are active connections
            std::string response_str = response.dump();
            {
                std::lock_guard<std::mutex> lock(connections_mutex);
                for (auto& [id, conn] : connections) {
                    std::lock_guard<std::mutex> conn_lock(conn->mutex);
                    conn->message_queue.push(response_str);
                    conn->cv.notify_one();
                }
            }
            
        } catch (const json::exception& e) {
            json error = create_error_response(-1, -32700, "Parse error: " + std::string(e.what()));
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        } catch (const std::exception& e) {
            json error = create_error_response(-1, -32603, "Internal error: " + std::string(e.what()));
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    });
    
    // Legacy /message endpoint for old HTTP+SSE transport compatibility
    server.Post("/message", [&](const httplib::Request& req, httplib::Response& res) {
        // Set CORS headers
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        
        try {
            // Parse incoming JSON-RPC message
            json request = json::parse(req.body);
            
            // Handle the request
            json response = handle_message(request);
            
            // Return JSON response
            res.set_content(response.dump(), "application/json");
            
            // Also broadcast via SSE if there are active connections
            std::string response_str = response.dump();
            {
                std::lock_guard<std::mutex> lock(connections_mutex);
                for (auto& [id, conn] : connections) {
                    std::lock_guard<std::mutex> conn_lock(conn->mutex);
                    conn->message_queue.push(response_str);
                    conn->cv.notify_one();
                }
            }
            
        } catch (const json::exception& e) {
            json error = create_error_response(-1, -32700, "Parse error: " + std::string(e.what()));
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        } catch (const std::exception& e) {
            json error = create_error_response(-1, -32603, "Internal error: " + std::string(e.what()));
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    });
    
    // CORS preflight
    server.Options("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Mcp-Session-Id, Accept");
        res.status = 204;
    });
    
    server.Options("/message", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 204;
    });
    
    std::cerr << "Server listening on http://localhost:" << port << std::endl;
    std::cerr << "MCP endpoint: http://localhost:" << port << "/" << std::endl;
    std::cerr << "Legacy endpoint: http://localhost:" << port << "/message" << std::endl;
    std::cerr << "Health check: http://localhost:" << port << "/health" << std::endl;
    std::cerr << "\nSupports both old HTTP+SSE (2024-11-05) and new Streamable HTTP transports" << std::endl;
    std::cerr << "\nTo test with MCP SDK client:" << std::endl;
    std::cerr << "  python test_mcp_sse.py --url http://localhost:" << port << std::endl;
    
    server.listen("127.0.0.1", port);  // Bind to localhost only for security
}

} // namespace mcp
