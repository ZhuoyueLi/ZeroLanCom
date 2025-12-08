// ServiceManager.h
#pragma once
#include <unordered_map>
#include <functional>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <string>
#include <zmq.hpp>
#include "utils/logger.hpp"
#include "utils/zmq_utils.hpp"
#include "utils/request_result.hpp"
#include "serialization/serializer.hpp"

namespace zlc {
class ServiceManager {
public:
    int service_port_;

    ServiceManager(const std::string& ip)
        : res_socket_(ZmqContext::instance(), zmq::socket_type::rep) {
        res_socket_.set(zmq::sockopt::rcvtimeo, SOCKET_TIMEOUT_MS);
        res_socket_.bind("tcp://" + ip + ":0");
        service_port_ = getBoundPort(res_socket_);
        LOG_INFO("[ServiceManager] ServiceManager bound to port {}", service_port_);
    };

    ~ServiceManager() {
        stop();
        res_socket_.close();
    }

    void start() {
        is_running = true;
        service_thread_ = std::thread(&ServiceManager::responseSocketThread, this);
    }

    void stop() {
        is_running = false;
        if (service_thread_.joinable()) {
            service_thread_.join();
        }
    }

    template <typename RequestType, typename ResponseType>
    void registerHandler(const std::string& name,
                        const std::function<ResponseType(const RequestType&)>& func)
    {
        handlers_[name] = [func](const ByteView& payload) -> std::vector<uint8_t>
        {
            RequestType req;
            decode(payload, req);
            ResponseType resp = func(req);
            ByteBuffer out;
            encode(resp, out);
            return std::vector<uint8_t>(out.data, out.data + out.size);
        };
    }

    template <typename RequestType>
    void registerHandler(const std::string& name,
                        const std::function<void(const RequestType&)>& func)
    {
        handlers_[name] = [func](const ByteView& payload) -> std::vector<uint8_t> {
            RequestType req;
            decode(payload, req);
            func(req);
            return {};
        };
    }

    template <typename ResponseType>
    void registerHandler(const std::string& name,
                        const std::function<ResponseType()>& func)
    {
        handlers_[name] = [func](const ByteView&) -> std::vector<uint8_t> {
            ResponseType ret = func();
            ByteView out;
            encode(ret, out);
            return std::vector<uint8_t>(out.data, out.data + out.size);
        };
    }

    // template <typename ClassT>
    void registerHandler(const std::string& name,
                        const std::function<void()>& func)
    {
        handlers_[name] = [func](const ByteView&) -> std::vector<uint8_t> {
            func();
            return {};
        };
    }


    void handleRequest(const std::string& service_name, const ByteView& payload, LanComResponse& response) {
        auto it = handlers_.find(service_name);
        LOG_INFO("[Lancom] Handling message of type {}", service_name);
        if (it == handlers_.end()) {
            // const std::string err = "Unknown handler";
            // protocol::FrankaResponse rr(protocol::RequestResultCode::FAIL, err);
            // std::vector<uint8_t> out = protocol::encode(rr);
            // std::vector<uint8_t> detail = protocol::encode(err);
            // out.insert(out.end(), detail.begin(), detail.end());
            // response = std::move(out);
            response.code = ResponseCode::FAIL;
            return;
        }
        response.code = ResponseCode::SUCCESS;
        try {
            response.payload = it->second(payload);
        } catch (const std::exception& e) {
            LOG_ERROR("[ServiceManager] Exception while handling {} service request: {}", service_name, e.what());
            response.code = ResponseCode::FAIL;
        }
        LOG_INFO("[ServiceManager] Found handler for service {}", service_name);
    }

    void clearHandlers() {
        handlers_.clear();
    }

    void removeHandler(const std::string& name) {
        handlers_.erase(name);
    }

        //response socket thread, used for service request
    void responseSocketThread() {
        while (is_running) {
            zmq::message_t service_name_msg;
            if (!res_socket_.recv(service_name_msg, zmq::recv_flags::none)) continue;
            LOG_TRACE("[ServiceManager] Received service request frame of size {} bytes.", service_name_msg.size());
            std::string service_name = decodeServiceHeader(ByteView{
                static_cast<const uint8_t*>(service_name_msg.data()),
                service_name_msg.size()
            });


            if (!service_name_msg.more()) {
                LOG_WARN("[ServiceManager] Warning: No payload frame received for service request.");
                continue; // Skip this iteration if no payload
            }
            zmq::message_t payload_msg;
            if (!res_socket_.recv(payload_msg, zmq::recv_flags::none)) continue;
            LOG_TRACE("[ServiceManager] Received payload frame of size {} bytes.", payload_msg.size());
            ByteView payload{
                static_cast<const uint8_t*>(payload_msg.data()),
                payload_msg.size()
            };
            if (payload_msg.more()) {
                LOG_WARN("[ServiceManager] Warning: More message frames received than expected.");
            }
            LOG_INFO("[ServiceManager] Received request {}", service_name);
            //std::string response;
            LanComResponse response;
            handleRequest(service_name, payload, response);
            //send response
            res_socket_.send(zmq::buffer(service_name), zmq::send_flags::sndmore);
            res_socket_.send(zmq::buffer(response.payload), zmq::send_flags::none);
            LOG_INFO("[ServiceManager] Sent response payload of {} bytes.", response.payload.size());
        }
    }

        ServiceManager(const ServiceManager&) = delete;
        ServiceManager& operator=(const ServiceManager&) = delete;

        ServiceManager(ServiceManager&&) = default;
        ServiceManager& operator=(ServiceManager&&) = default;


    private:
        std::unordered_map<std::string, std::function<std::vector<uint8_t>(const ByteView&)>> handlers_;
        bool is_running{false};
        zmq::socket_t res_socket_{ZmqContext::instance(), zmq::socket_type::rep};
        static constexpr int SOCKET_TIMEOUT_MS = 100;
        std::thread service_thread_;
        std::string service_addr_;
    };
}