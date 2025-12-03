#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include <optional>
#include <iostream>
#include <shared_mutex>
#include "nodes/node_info.hpp"
#include "utils/logger.hpp"


namespace lancom {

class NodeInfoManager {
private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, NodeInfo> nodes_info;
    std::unordered_map<std::string, uint32_t> nodes_info_id;
    std::unordered_map<std::string, std::string> nodes_heartbeat;
    
    void update_node_unlocked(const std::string& nodeID, const NodeInfo& info) {
        nodes_info[nodeID] = info;
        nodes_info_id[nodeID] = info.infoID;
        nodes_heartbeat[nodeID] = info.ip;
    }

    bool check_node_id_unlocked(const std::string& nodeID) const {
        return nodes_info.find(nodeID) != nodes_info.end();
    }

    bool check_node_info_id_unlocked(const std::string& nodeID, uint32_t infoID) const {
        auto it = nodes_info_id.find(nodeID);
        if (it == nodes_info_id.end()) return false;
        return it->second == infoID;
    }


public:

    bool check_node_id(const std::string& nodeID) const {
        std::shared_lock lock(mutex_);
        return check_node_id_unlocked(nodeID);
    }

    bool check_node_info_id(const std::string& nodeID, uint32_t infoID) const {
        std::shared_lock lock(mutex_);
        return check_node_info_id_unlocked(nodeID, infoID);
    }

    // bool check_multicast_message(const std::string& nodeID, uint32_t infoID) const {
    //     return  check_node_id(nodeID) && check_node_info_id(nodeID, infoID);
    // }

    void remove_node(const std::string& nodeID) {
        std::unique_lock lock(mutex_);
        nodes_info.erase(nodeID);
        nodes_info_id.erase(nodeID);
        nodes_heartbeat.erase(nodeID);
    }

    std::vector<SocketInfo> get_publisher_info(const std::string& topicName) const {
        std::shared_lock lock(mutex_);
        std::vector<SocketInfo> result;
        for (auto& [id, node] : nodes_info) {
            for (auto& t : node.topics) {
                if (t.name == topicName)
                    result.push_back(t);
            }
        }
        return result;
    }

    std::optional<SocketInfo> get_service_info(const std::string& serviceName) const {
        std::shared_lock lock(mutex_);
        for (auto& [id, node] : nodes_info) {
            for (auto& t : node.services) {
                if (t.name == serviceName)
                    return t;
            }
        }
        return std::nullopt;
    }

    void process_heartbeat(const NodeInfo& nodeInfo) {
        try {
            bool is_new = false;
            bool info_changed = false;

            {
                std::unique_lock lock(mutex_);

                if (!check_node_id_unlocked(nodeInfo.nodeID)) {
                    update_node_unlocked(nodeInfo.nodeID, nodeInfo);
                    is_new = true;
                }
                else if (!check_node_info_id_unlocked(nodeInfo.nodeID, nodeInfo.infoID)) {
                    update_node_unlocked(nodeInfo.nodeID, nodeInfo);
                    info_changed = true;
                }
            }
            // spdlog logging outside lock since it has its own internal locking
            if (is_new) {
                LOG_INFO("Node {} added via heartbeat", nodeInfo.name);
                nodeInfo.printNodeInfo();
            }
            else if (info_changed) {
                LOG_INFO("Node {} updated via heartbeat", nodeInfo.name);
                nodeInfo.printNodeInfo();
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error processing heartbeat: " << e.what() << "\n";
        }
    }

};

} // namespace lancom