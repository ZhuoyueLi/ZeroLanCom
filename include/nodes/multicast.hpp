#pragma once
#include <string>
#include <vector>
#include <thread>
#include <functional>
#include <cstring>
#include <arpa/inet.h>
#include <thread>
#include <atomic>
#include "nodes/node_info.hpp"
#include "nodes/node_info_manager.hpp"
#include "utils/logger.hpp"

namespace zlc {

class MulticastSender {
public:
    int sock;
    sockaddr_in addr;

    MulticastSender(const std::string& group, int port, const std::string& localIP) {
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        int ttl = 1;
        setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

        in_addr local;
        local.s_addr = inet_addr(localIP.c_str());
        setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &local, sizeof(local));

        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(group.c_str());
    }

    
    void start(const LocalNodeInfo& localInfo) {
        multicastSendThread = std::thread([this, &localInfo](){
            running_ = true;
            // LOG_INFO("Multicast sender started.");
            while (running_) {
                auto msg = localInfo.createHeartbeat();
                send(msg);
                std::this_thread::sleep_for(std::chrono::seconds(1));
                LOG_TRACE("Sent multicast heartbeat from node {}", localInfo.nodeID);
            }
        });
    }
    
    void stop() {
        running_ = false;
        if (multicastSendThread.joinable()) {
            multicastSendThread.join();
        }
    }

private:
    
    std::thread multicastSendThread;
    std::atomic<bool> running_;
    void send(const std::vector<uint8_t>& msg) {
        sendto(sock, msg.data(), msg.size(), 0,
               (sockaddr*)&addr, sizeof(addr));
    }
};


class MulticastReceiver {
public:
    MulticastReceiver(const std::string& group, int port, const std::string& localIP)
        : running_(false)
    {
        sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        int reuse = 1;
        setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        setsockopt(sock_, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        bind(sock_, (sockaddr*)&addr, sizeof(addr));

        ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr(group.c_str());
        mreq.imr_interface.s_addr = inet_addr(localIP.c_str());
        setsockopt(sock_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    }

    ~MulticastReceiver() {
        stop();
    }

    void start(NodeInfoManager& nodeManager) {
        running_ = true;
        multicastReceiveThread = std::thread([this, &nodeManager]() {

            std::vector<uint8_t> buf(256);  // safer than uint8_t[128]
            sockaddr_in src{};
            socklen_t slen = sizeof(src);
            // LOG_INFO("Multicast receiver started.");
            while (running_) {
                int n = recvfrom(sock_, buf.data(), (int)buf.size(), 0, (sockaddr*)&src, &slen);      
                if (n <= 0) continue;
                std::string ip = inet_ntoa(src.sin_addr);
                NodeInfo info = NodeInfo::decode(ByteView{buf.data(), (size_t)n});
                info.ip = ip;
                LOG_TRACE("Received multicast heartbeat from node {} at IP {}", info.nodeID, ip);
                nodeManager.processHeartbeat(info);
                nodeManager.checkHeartbeats();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            LOG_INFO("Multicast receiver stopped.");
        });
    }

    void stop() {
        running_ = false;

        if (multicastReceiveThread.joinable())
            multicastReceiveThread.join();
    }

private:
    int sock_;
    std::thread multicastReceiveThread;
    std::atomic<bool> running_;
};

} // namespace zlc