#pragma once

#include <memory>
#include <string>
#include <functional>
#include <zmq.hpp>
#include <msgpack.hpp>
#include "nodes/node_info_manager.hpp"


template<typename T>
class Subscriber {
public:
    // Create a new subscriber
    Subscriber(
        const std::string& topic_name,
        std::function<const T&> callback,
        bool with_local_namespace = false
    ) {
        std::string full_topic_name = with_local_namespace ?
            "lc.local." + topic_name : topic_name;
        socket_ = std::make_unique<zmq::socket_t>(
            ZmqContext::instance(), zmq::socket_type::sub);
        socket_->connect("tcp://localhost:" + std::to_string(port_));
        socket_->set(zmq::sockopt::subscribe, full_topic_name);
        LOG_INFO("[Subscriber] Subscriber for topic '{}' connected to port {}",
                 full_topic_name, port_);
    };
    // Destructor
    ~Subscriber() = default;
    // Receive methods for different types
    void processMessage() {
        zmq::message_t message;
        socket_->recv(message, zmq::recv_flags::none);
        msgpack::object_handle oh = msgpack::unpack(
            static_cast<const char*>(message.data()), message.size());
        msgpack::object obj = oh.get();
        T msg;
        obj.convert(msg);
        callback_(msg);
    };

    void wait_for_topic() {
        // find the node info from the node manager
        while (true) {
            zmq::message_t message;
            auto result = socket_->recv(message, zmq::recv_flags::dontwait);
            if (result) {
                // Message received
                return;
            }
        }
    }

private:
    std::unique_ptr<zmq::socket_t> socket_;
    std::function<const T&> callback_;
    NodeInfoManager& node_manager_;
};