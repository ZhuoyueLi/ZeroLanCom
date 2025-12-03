#pragma once
#include <zmq.hpp>
#include <string>

class ZmqContext {
public:
    static zmq::context_t& instance() {
        static zmq::context_t ctx{1};
        return ctx;
    }

    ZmqContext() = delete;
};

int get_bound_port(zmq::socket_t& socket) {
    // fetch endpoint string using modern cppzmq API
    std::string endpoint = socket.get(zmq::sockopt::last_endpoint);

    auto pos = endpoint.rfind(':');
    if (pos == std::string::npos) {
        throw std::runtime_error("Invalid endpoint: " + endpoint);
    }

    return std::stoi(endpoint.substr(pos + 1));
}
