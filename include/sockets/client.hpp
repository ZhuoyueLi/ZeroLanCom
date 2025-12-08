#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "utils/logger.hpp"
#include "serialization/msppack_codec.hpp"
#include "serialization/serializer.hpp"
#include "zerolancom_node.hpp"

namespace zlc {

// Service proxy for making remote procedure calls
class Client {
public:
    // Make a service request
    template<typename RequestType, typename ResponseType>
    static void request(
        const std::string& service_name,
        const RequestType& request,
        ResponseType& response){

        auto& node = ZeroLanComNode::instance();
        
        // Find service in the nodes map
        auto serviceInfoPtr = node.nodesManager.getServiceInfo(service_name);
        
        if (serviceInfoPtr == nullptr) {
            LOG_ERROR("Service " + service_name + " is not available");
            return;
        }
        const SocketInfo& serviceInfo = *serviceInfoPtr;
        LOG_INFO("[Client] Found service '{}' at {}:{}", 
                service_name, serviceInfo.ip, serviceInfo.port);
        // Create ZMQ request socket
        zmq::socket_t req_socket(ZmqContext::instance(), zmq::socket_type::req);
        req_socket.connect("tcp://" + serviceInfo.ip + ":" + std::to_string(serviceInfo.port));
        LOG_INFO("[Client] Connected to service '{}' at {}:{}", 
                service_name, serviceInfo.ip, serviceInfo.port);
        ByteBuffer out;
        encode(request, out);
        // Send service name and request payload
        req_socket.send(zmq::buffer(service_name), zmq::send_flags::sndmore);
        req_socket.send(zmq::buffer(out.data, out.size), zmq::send_flags::none);
        LOG_INFO("[Client] Sent request to service '{}'", service_name);
        // Receive response
        zmq::message_t service_name_msg;
        if (!req_socket.recv(service_name_msg, zmq::recv_flags::none)) {
            LOG_ERROR("Timeout waiting for response from service " + service_name);
            return;
        }
        LOG_TRACE("[Client] Received service name frame of size {} bytes.", service_name_msg.size());
        if (!service_name_msg.more()) {
            LOG_ERROR("No payload frame received for service response from " + service_name);
            return;
        }
        zmq::message_t payload_msg;
        if (!req_socket.recv(payload_msg, zmq::recv_flags::none)) {
            LOG_ERROR("Timeout waiting for payload from service " + service_name);
            return;
        }
        LOG_TRACE("[Client] Received payload frame of size {} bytes.", payload_msg.size());
        if (payload_msg.more()) {
            LOG_ERROR("More message frames received than expected from service " + service_name);
        }
        ByteView payload{
            static_cast<const uint8_t*>(payload_msg.data()),
            payload_msg.size()
        };
        decode(payload, response);
        LOG_INFO("[Client] Received response from service '{}'", service_name);
    }

    static void waitForService(
        const std::string& service_name,
        int max_wait_ms = 5000,
        int check_interval_ms = 100)
        {
            auto& node = ZeroLanComNode::instance();
            int waited_ms = 0;
            while (waited_ms < max_wait_ms) {
                auto serviceInfoPtr = node.nodesManager.getServiceInfo(service_name);
                if (serviceInfoPtr != nullptr) {
                    LOG_INFO("[Client] Service '{}' is now available.", service_name);
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms));
                waited_ms += check_interval_ms;
            }
        }
};

// Template implementation

} // namespace zlc
