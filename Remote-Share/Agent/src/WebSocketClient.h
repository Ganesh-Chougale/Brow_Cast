#pragma once
#include <string>
#include <vector>
#include <functional> 
#include <thread>     
#include <atomic>     
#define ASIO_STANDALONE 
#include <websocketpp/config/asio_no_tls_client.hpp> 
#include <websocketpp/client.hpp>
#include <websocketpp/common/thread.hpp> 
#include <websocketpp/common/memory.hpp> 
#include <nlohmann/json.hpp>
typedef websocketpp::client<websocketpp::config::asio_client> client;
class WebSocketClient {
public:
    WebSocketClient(const std::string& uri);
    ~WebSocketClient();
    void connect();
    void send(const std::string& message_payload);
    bool isConnected() const;
    void setOnOpenHandler(std::function<void()> handler);
    void setOnCloseHandler(std::function<void()> handler);
    void setOnMessageHandler(std::function<void(const std::string&)> handler);
private:
    client m_client; 
    websocketpp::connection_hdl m_hdl; 
    std::string m_uri; 
    std::atomic<bool> m_connected; 
    websocketpp::lib::thread m_thread; 
    void onOpen(websocketpp::connection_hdl hdl);
    void onClose(websocketpp::connection_hdl hdl);
    void onMessage(websocketpp::connection_hdl hdl, client::message_ptr msg);
    void onFail(websocketpp::connection_hdl hdl);
    std::function<void()> m_onOpenHandler;
    std::function<void()> m_onCloseHandler;
    std::function<void(const std::string&)> m_onMessageHandler;
};