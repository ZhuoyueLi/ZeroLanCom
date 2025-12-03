#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <zmq.hpp>

#include <msgpack.hpp>

#include "lancom_node.hpp"

namespace lancom {

template<typename T>
class Publisher {
public:
    // Create a new publisher
    Publisher(
        const std::string& topic_name,
        bool with_local_namespace = false) {
        std::string full_topic_name = with_local_namespace ?
            "lc.local." + topic_name : topic_name;
        socket_ = std::make_unique<zmq::socket_t>(
            ZmqContext::instance(), zmq::socket_type::pub);
        const std::string address = LanComNode::instance().GetIP();
        socket_->bind("tcp://" + address + ":0");
        LOG_INFO("[Publisher] Publisher for topic '{}' bound to port {}",
                 full_topic_name, get_bound_port(*socket_));
        port_ = get_bound_port(*socket_);
        LanComNode::instance().registerTopic(
            full_topic_name, static_cast<uint16_t>(port_)
    );
    };
    // Destructor
    ~Publisher() = default;
    
    // Publish methods for different types
    void publish(const T& msg) {
        msgpack::sbuffer sbuf;
        msgpack::pack(sbuf, msg);
        socket_->send(zmq::buffer(sbuf.data(), sbuf.size()), zmq::send_flags::none);
    };
    
    // Implement shutdown
    void on_shutdown();
    
    // Socket reference
    std::unique_ptr<zmq::socket_t> socket_;
    int port_;
};

} // namespace lancom
