#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "utils/logger.hpp"
#include "utils/binary_codec.hpp"
#include "lancom_node.hpp"

namespace lancom {

// Service proxy for making remote procedure calls
class LanComClient {
public:
    // Make a service request
    template<typename RequestType, typename ResponseType>
    static std::optional<ResponseType> request(
        const std::string& service_name,
        const RequestType& request);

    static void waitForService(
        const std::string& service_name,
        int max_wait_ms = 5000,
        int check_interval_ms = 100)
        {
            auto& node = LanComNode::instance();
            int waited_ms = 0;
            while (waited_ms < max_wait_ms) {
                auto serviceInfoPtr = node.nodesManager.getServiceInfo(service_name);
                if (serviceInfoPtr != nullptr) {
                    LOG_INFO("[LanComClient] Service '{}' is now available.", service_name);
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms));
                waited_ms += check_interval_ms;
            }
        }
};

// Template implementation
template<typename RequestType, typename ResponseType>
std::optional<ResponseType> LanComClient::request(
    const std::string& service_name,
    const RequestType& request) {

    auto& node = LanComNode::instance();
    
    // Find service in the nodes map
    auto serviceInfoPtr = node.nodesManager.getServiceInfo(service_name);
    
    if (serviceInfoPtr == nullptr) {
        LOG_ERROR("Service " + service_name + " is not available");
        return std::nullopt;
    }
    const SocketInfo& serviceInfo = *serviceInfoPtr;
    LOG_INFO("[LanComClient] Found service '{}' at {}:{}", 
             service_name, serviceInfo.ip, serviceInfo.port);
    // Create ZMQ request socket
    zmq::socket_t req_socket(ZmqContext::instance(), zmq::socket_type::req);
    req_socket.connect("tcp://" + serviceInfo.ip + ":" + std::to_string(serviceInfo.port));
    LOG_INFO("[LanComClient] Connected to service '{}' at {}:{}", 
             service_name, serviceInfo.ip, serviceInfo.port);
    // Serialize request using msgpack
    msgpack::sbuffer sbuf;
    msgpack::pack(sbuf, request);
    // Send service name and request payload
    req_socket.send(zmq::buffer(service_name), zmq::send_flags::sndmore);
    req_socket.send(zmq::buffer(sbuf.data(), sbuf.size()), zmq::send_flags::none);
    LOG_INFO("[LanComClient] Sent request to service '{}'", service_name);
    // Receive response
    zmq::message_t service_name_msg;
    if (!req_socket.recv(service_name_msg, zmq::recv_flags::none)) {
        LOG_ERROR("Timeout waiting for response from service " + service_name);
        return std::nullopt;
    }
    LOG_TRACE("[LanComClient] Received service name frame of size {} bytes.", service_name_msg.size());
    if (!service_name_msg.more()) {
        LOG_ERROR("No payload frame received for service response from " + service_name);
        return std::nullopt;
    }
    zmq::message_t payload_msg;
    if (!req_socket.recv(payload_msg, zmq::recv_flags::none)) {
        LOG_ERROR("Timeout waiting for payload from service " + service_name);
        return std::nullopt;
    }
    LOG_TRACE("[LanComClient] Received payload frame of size {} bytes.", payload_msg.size());
    if (payload_msg.more()) {
        LOG_ERROR("More message frames received than expected from service " + service_name);
    }
    // Deserialize response using msgpack
    ByteView payload{
        static_cast<const uint8_t*>(payload_msg.data()),
        payload_msg.size()
    };
    msgpack::object_handle oh = msgpack::unpack(
        reinterpret_cast<const char*>(payload.data),
        payload.size
    );

}
} // namespace lancom
