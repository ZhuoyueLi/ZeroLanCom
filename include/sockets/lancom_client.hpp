#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "lancom/nodes/lancom_node.hpp"
#include "lancom/types.hpp"
#include "lancom/utils/node_utils.hpp"

namespace lancom {

// Service proxy for making remote procedure calls
class LanComClient {
public:
    // Make a service request
    template<typename Req, typename Resp>
    static std::optional<Resp> request(
        const std::string& service_name,
        std::function<BytesMessage(const Req&)> request_encoder,
        std::function<Resp(const BytesMessage&)> response_decoder,
        const Req& request);
    void waitForService(
        const std::string& service_name,
        int max_wait_ms = 5000,
        int check_interval_ms = 100);

    };

// Template implementation
template<typename RequestType, typename ResponseType>
std::optional<ResponseType> LanComClient::request(
    const std::string& service_name,
    std::function<BytesMessage(const RequestType&)> request_encoder,
    std::function<ResponseType(const BytesMessage&)> response_decoder,
    const RequestType& request) {
    
    // Check if node is initialized
    if (!LanComNode::instance) {
        throw std::runtime_error("LanCom Node is not initialized");
    }
    
    auto node = LanComNode::instance;
    
    // Find service in the nodes map
    auto service_component = node->nodes_map_->get_service_info(service_name);
    
    if (service_component.socketID.empty()) {
        warning("Service " + service_name + " is not available");
        return std::nullopt;
    }
    
    // Encode request
    BytesMessage request_bytes = request_encoder(request);
    
    // Create address
    std::string addr = "tcp://" + service_component.ip + ":" + 
                       std::to_string(service_component.port);
    
    // Send request
    auto response_future = node->submit_task([addr, service_name, request_bytes]() {
        return utils::send_bytes_request(addr, service_name, request_bytes);
    }, true);
    
    // Decode response
    return response_decoder(response_future);
}

} // namespace lancom
