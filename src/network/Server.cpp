#include "Server.h"
#include "core/Log.h"
#include <httplib.h>

Server::Server() = default;
Server::~Server() { Stop(); }

bool Server::Start(int port) {
    m_port = port;
    m_running = true;
    m_thread = std::make_unique<std::thread>(&Server::Worker, this);
    Log::Info("HTTP server starting on port %d", port);
    return true;
}

void Server::Stop() {
    m_running = false;
    if (m_thread && m_thread->joinable())
        m_thread->join();
    m_thread.reset();
    Log::Info("HTTP server stopped");
}

void Server::OnRequest(RequestHandler handler) {
    m_handler = std::move(handler);
}

bool Server::IsRunning() const {
    return m_running && m_thread && m_thread->joinable();
}

void Server::Worker() {
    httplib::Server svr;

    svr.Post("/compile", [this](const httplib::Request& req, httplib::Response& res) {
        if (!m_handler) {
            res.status = 500;
            res.set_content("No handler", "text/plain");
            return;
        }
        auto result = m_handler("POST", "/compile", req.body);
        res.set_content(result, "text/plain");
    });

    svr.Post("/inject", [this](const httplib::Request& req, httplib::Response& res) {
        if (!m_handler) {
            res.status = 500;
            res.set_content("No handler", "text/plain");
            return;
        }
        auto result = m_handler("POST", "/inject", req.body);
        res.set_content(result, "text/plain");
    });

    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("ok", "text/plain");
    });

    svr.listen("0.0.0.0", m_port);
}
