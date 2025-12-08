#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <zmq.hpp>

#include "utils/zmq_utils.hpp"
#include "utils/logger.hpp"
#include "serialization/serializer.hpp"

namespace zlc {

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
        const std::string address = ZeroLanComNode::instance().GetIP();
        socket_->bind("tcp://" + address + ":0");
        LOG_INFO("[Publisher] Publisher for topic '{}' bound to port {}",
                 full_topic_name, getBoundPort(*socket_));
        port_ = getBoundPort(*socket_);
        ZeroLanComNode::instance().registerTopic(
            full_topic_name, static_cast<uint16_t>(port_)
    );
    };
    // Destructor
    ~Publisher() = default;
    
    // Publish methods for different types
    void publish(const T& msg) {
        ByteBuffer out;
        encode(msg, out);
        socket_->send(zmq::buffer(out.data, out.size), zmq::send_flags::none);
    };
    
    // Implement shutdown
    void on_shutdown();
    
    // Socket reference
    std::unique_ptr<zmq::socket_t> socket_;
    int port_;
};

} // namespace zlc
