#include "NetworkService.h"
#include "core/Log.h"
#include <httplib.h>

struct NetworkService::Impl {
    httplib::Server svr;
    RequestHandler handler;
    std::thread worker;
    bool running{false};

    void Route() {
        svr.Post("/compile", [this](const httplib::Request& req, httplib::Response& res) {
            if (!handler) { res.status = 500; res.set_content("No handler", "text/plain"); return; }
            res.set_content(handler("POST", "/compile", req.body), "text/plain");
        });
        svr.Post("/inject", [this](const httplib::Request& req, httplib::Response& res) {
            if (!handler) { res.status = 500; res.set_content("No handler", "text/plain"); return; }
            res.set_content(handler("POST", "/inject", req.body), "text/plain");
        });
        svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
            res.set_content("ok", "text/plain");
        });
    }
};

NetworkService::NetworkService() : m_impl(std::make_unique<Impl>()) {}
NetworkService::~NetworkService() { Stop(); }

bool NetworkService::Start(int port) {
    m_impl->Route();
    m_impl->running = true;
    m_impl->worker = std::thread([this, port]() {
        m_impl->svr.listen("0.0.0.0", port);
    });
    Log::Info("NetworkService started on port %d", port);
    return true;
}

void NetworkService::Stop() {
    if (m_impl->running) {
        m_impl->svr.stop();
        if (m_impl->worker.joinable()) m_impl->worker.join();
        m_impl->running = false;
        Log::Info("NetworkService stopped");
    }
}

void NetworkService::SetHandler(RequestHandler handler) {
    m_impl->handler = std::move(handler);
}
