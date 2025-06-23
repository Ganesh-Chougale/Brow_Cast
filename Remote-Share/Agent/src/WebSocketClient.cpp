#include "WebSocketClient.h"
#include <iostream>

WebSocketClient::WebSocketClient(const std::string& uri)
    : m_uri(uri), m_connected(false) {
    m_client.init_asio();

    // Suppress verbose access log channels, keep error channels
    m_client.set_access_channels(websocketpp::log::alevel::none);
    m_client.set_error_channels(websocketpp::log::elevel::all);

    // Set handlers for various connection events
    m_client.set_open_handler(websocketpp::lib::bind(
        &WebSocketClient::onOpen,
        this,
        websocketpp::lib::placeholders::_1
    ));
    m_client.set_close_handler(websocketpp::lib::bind(
        &WebSocketClient::onClose,
        this,
        websocketpp::lib::placeholders::_1
    ));
    m_client.set_message_handler(websocketpp::lib::bind(
        &WebSocketClient::onMessage,
        this,
        websocketpp::lib::placeholders::_1,
        websocketpp::lib::placeholders::_2
    ));
    m_client.set_fail_handler(websocketpp::lib::bind(
        &WebSocketClient::onFail,
        this,
        websocketpp::lib::placeholders::_1
    ));
}

WebSocketClient::~WebSocketClient() {
    // Only attempt to close if connected and the handle is still valid
    if (m_connected.load()) {
        websocketpp::lib::error_code ec;
        if (m_hdl.expired()) {
            std::cerr << "WebSocketClient destructor: Connection handle expired, skipping close." << std::endl;
        } else {
            // Attempt a graceful shutdown
            m_client.close(m_hdl, websocketpp::close::status::going_away, "Client shutting down", ec);
            if (ec) {
                std::cerr << "Error closing WebSocket: " << ec.message() << std::endl;
            }
        }
    }

    // Stop the client's internal ASIO loop
    m_client.stop();

    // Join the thread to ensure it finishes before destructor exits
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void WebSocketClient::connect() {
    websocketpp::lib::error_code ec;
    client::connection_ptr con = m_client.get_connection(m_uri, ec);
    if (ec) {
        std::cerr << "Could not create connection because: " << ec.message() << std::endl;
        throw websocketpp::exception(ec.message()); // Rethrow as websocketpp exception
    }

    // Connect the connection
    m_client.connect(con);

    // Start the ASIO io_service in a separate thread if not already running
    if (!m_thread.joinable()) {
        m_thread = websocketpp::lib::thread([&]() {
            try {
                m_client.run();
            } catch (const websocketpp::exception& e) {
                std::cerr << "WebSocket client run exception: " << e.what() << std::endl;
            } catch (const std::exception& e) {
                // Catch any other standard exceptions
                std::cerr << "General exception in WebSocket client run thread: " << e.what() << std::endl;
            } catch (...) {
                // Catch any other unknown exceptions
                std::cerr << "Unknown exception in WebSocket client run thread." << std::endl;
            }
        });
    }
}

void WebSocketClient::send(const std::string& message_payload) {
    // Only send if connected and the handle is still valid
    if (m_connected.load() && !m_hdl.expired()) {
        websocketpp::lib::error_code ec;
        m_client.send(m_hdl, message_payload, websocketpp::frame::opcode::text, ec);
        if (ec) {
            std::cerr << "Error sending message: " << ec.message() << std::endl;
            // Optionally, handle reconnection or state change here if send fails
        }
    } else {
        // std::cerr << "Attempted to send message, but WebSocket is not connected or handle expired." << std::endl;
    }
}

bool WebSocketClient::isConnected() const {
    // A robust check: atomic flag and handle validity
    return m_connected.load() && !m_hdl.expired();
}

void WebSocketClient::setOnOpenHandler(std::function<void()> handler) {
    m_onOpenHandler = handler;
}

void WebSocketClient::setOnCloseHandler(std::function<void()> handler) {
    m_onCloseHandler = handler;
}

void WebSocketClient::setOnMessageHandler(std::function<void(const std::string&)> handler) {
    m_onMessageHandler = handler;
}

// Handler implementations
void WebSocketClient::onOpen(websocketpp::connection_hdl hdl) {
    // std::cout << "WebSocket Connected!" << std::endl; // Already handled by onOpenHandler callback
    m_hdl = hdl; // Store the connection handle
    m_connected.store(true); // Atomically set connected state

    if (m_onOpenHandler) {
        m_onOpenHandler(); // Call the user-defined callback
    }
}

void WebSocketClient::onClose(websocketpp::connection_hdl hdl) {
    // std::cout << "WebSocket Disconnected." << std::endl; // Already handled by onCloseHandler callback
    m_connected.store(false); // Atomically set disconnected state

    if (m_onCloseHandler) {
        m_onCloseHandler(); // Call the user-defined callback
    }
}

void WebSocketClient::onMessage(websocketpp::connection_hdl hdl, client::message_ptr msg) {
    if (m_onMessageHandler) {
        m_onMessageHandler(msg->get_payload()); // Call the user-defined callback with message payload
    }
}

void WebSocketClient::onFail(websocketpp::connection_hdl hdl) {
    client::connection_ptr con = m_client.get_con_from_hdl(hdl);
    std::cerr << "WebSocket connection failed: " << con->get_ec().message() << std::endl;
    m_connected.store(false); // Update state

    // Unify connection loss handling: call onCloseHandler
    if (m_onCloseHandler) {
        m_onCloseHandler();
    }
}