#pragma once
#include <vector>
#include <cstring>
#include <cstdint>
#include <arpa/inet.h>
#include "utils/binary_codec.hpp"
#include "utils/message.hpp"
#include "utils/logger.hpp"
// =======================
// SocketInfo
// =======================

namespace lancom {

struct SocketInfo {
    std::string name;
    uint16_t port;

    void encode(BinWriter& w) const {
        w.write_string(name);
        w.write_u16(port);
    }

    static SocketInfo decode(BinReader& r) {
        SocketInfo si;
        si.name = r.read_string();
        si.port = r.read_u16();
        return si;
    }

    void createHeartbeat(std::vector<uint8_t>& buf) const {
        uint8_t* p = buf.data();
        uint16_t name_len = htons((uint16_t)name.size());
        memcpy(p, &name_len, 2); p += 2;
        memcpy(p, name.data(), name.size()); p += name.size();
        uint16_t port_net = htons(port);
        memcpy(p, &port_net, 2);
    }

};

// =======================
// NodeInfo
// =======================

struct NodeInfo {
    std::string nodeID;     // 36-char UUID
    uint32_t infoID;
    std::string name;
    std::string ip;
    std::vector<SocketInfo> topics;
    std::vector<SocketInfo> services;

    std::vector<uint8_t> encode() const {
        BinWriter w;

        w.write_fixed_string(nodeID, 36);
        w.write_u32(infoID);
        w.write_string(name);
        w.write_string(ip);

        w.write_u16(topics.size());
        for (auto& t : topics) t.encode(w);

        w.write_u16(services.size());
        for (auto& s : services) s.encode(w);

        return std::move(w.buf);
    }

    static NodeInfo decode(ByteView bv) {
        BinReader r{bv};
        NodeInfo ni;

        ni.nodeID = r.read_fixed_string(36);
        ni.infoID = r.read_u32();
        ni.name   = r.read_string();
        ni.ip     = r.read_string();

        uint16_t topics_count = r.read_u16();
        ni.topics.reserve(topics_count);
        for (int i = 0; i < topics_count; i++)
            ni.topics.push_back(SocketInfo::decode(r));

        uint16_t srv_count = r.read_u16();
        ni.services.reserve(srv_count);
        for (int i = 0; i < srv_count; i++)
            ni.services.push_back(SocketInfo::decode(r));

        return ni;
    }

    void printNodeInfo() const {
        LOG_INFO("NodeID: {}", nodeID);
        LOG_INFO("InfoID: {}", infoID);
        LOG_INFO("Name: {}", name);
        LOG_INFO("IP: {}", ip);
        LOG_INFO("Topics:");
        for (const auto& t : topics) {
            LOG_INFO("  - {}:{}", t.name, t.port);
        }
        LOG_INFO("Services:");
        for (const auto& s : services) {
            LOG_INFO("  - {}:{}", s.name, s.port);
        }
    }

};

struct LocalNodeInfo {
    std::string nodeID;
    NodeInfo nodeInfo;

    LocalNodeInfo(const std::string& name, const std::string& ip)
    {
        nodeID = generateUUID();
        nodeInfo.nodeID = nodeID;
        nodeInfo.infoID = 0;
        nodeInfo.name = name;
        nodeInfo.ip = ip;
    }

    std::vector<uint8_t> createHeartbeat() const {
        return nodeInfo.encode();
    }

    void registerTopic(const std::string& name, uint16_t port) {
        nodeInfo.topics.push_back(SocketInfo{name, port});
        nodeInfo.infoID++;
    }

    void registerServices(const std::string& name, uint16_t port) {
        nodeInfo.services.push_back(SocketInfo{name, port});
        nodeInfo.infoID++;
    }
};
}