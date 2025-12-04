#pragma once
#include <unordered_map>
#include <functional>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <zmq.hpp>
#include <fmt/format.h>
#include "utils/logger.hpp"
#include "utils/binary_codec.hpp"
#include "utils/zmq_utils.hpp"
#include "nodes/node_info_manager.hpp"

namespace lancom {

class SubscriberManager {
public:
    SubscriberManager(NodeInfoManager& node_info_mgr)
        : node_info_mgr_(node_info_mgr) {
        node_info_mgr_.registerUpdateCallback(
            std::bind(&SubscriberManager::updateTopicSubscriber, this, std::placeholders::_1)
        );
    }

    ~SubscriberManager() {
        stop();
    }

    // ---------------------------------------------------------------------
    // Register topic-subscriber: automatically discover all publishers
    // ---------------------------------------------------------------------
    template<typename MessageType>
    void registerTopicSubscriber(
        const std::string& topicName,
        const std::function<void(const MessageType&)>& callback)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        Subscriber sub;
        sub.topicName = topicName;
        sub.callback = [callback](const ByteView& view) {
            // decode message
            msgpack::object_handle oh = msgpack::unpack(
                reinterpret_cast<const char*>(view.data),
                view.size
            );
            MessageType msg;
            oh.get().convert(msg);
            // call user callback
            callback(msg);
        };
        sub.socket = std::make_unique<zmq::socket_t>(ZmqContext::instance(), zmq::socket_type::sub);
        sub.socket->set(zmq::sockopt::subscribe, "");

        // initial URL list
        auto urls = findTopicURLs(topicName);
        auto& subURLs = sub.publisherURLs;
        for (auto& url : urls) {
            if (std::find(subURLs.begin(), subURLs.end(), url) != subURLs.end())
                continue;
            sub.socket->connect(url);
            LOG_INFO("[SubscriberManager] '{}' connected to {}", topicName, url);
        }
        subscribers_.push_back(std::move(sub));
    }

    // ---------------------------------------------------------------------
    // Find all publisher URLs for a topic
    // ---------------------------------------------------------------------
    std::vector<std::string> findTopicURLs(const std::string& topicName) {
        std::vector<std::string> urls;
        auto infos = node_info_mgr_.getPublisherInfo(topicName);
        for (auto& t : infos) {
            urls.push_back(fmt::format("tcp://{}:{}", t.ip, t.port));
        }
        return urls;
    }

    // ---------------------------------------------------------------------
    // Called by NodeInfoManager when NodeInfo updated
    // ---------------------------------------------------------------------
    void updateTopicSubscriber(const NodeInfo& nodeInfo) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& topic : nodeInfo.topics) {
            for (auto it = subscribers_.begin(); it != subscribers_.end(); ++it) {
                if (it->topicName != topic.name) {
                    continue;
                }
                std::string url = fmt::format("tcp://{}:{}", topic.ip, topic.port);
                auto& subURLs = it->publisherURLs;
                if (std::find(subURLs.begin(), subURLs.end(), url) != subURLs.end()) {
                    continue; // already connected
                }
                it->socket->connect(url);
                LOG_INFO("[SubscriberManager] '{}' connected to {}", topic.name, url);
                subURLs.push_back(url);
            }
        }
    }

    // ---------------------------------------------------------------------
    void start() {
        is_running_ = true;
        poll_thread_ = std::thread(&SubscriberManager::pollLoop, this);
    }

    void stop() {
        is_running_ = false;
        if (poll_thread_.joinable()) poll_thread_.join();
    }

private:
    struct Subscriber {
        std::string topicName;
        std::vector<std::string> publisherURLs;
        std::function<void(const ByteView&)> callback;
        std::unique_ptr<zmq::socket_t> socket;
    };

    void pollLoop() {
        LOG_INFO("[SubscriberManager] Poll thread started.");
        while (is_running_) {

            std::vector<zmq::pollitem_t> poll_items;
            std::vector<Subscriber*> subs;
            {
                std::lock_guard<std::mutex> lk(mutex_);
                poll_items.reserve(subscribers_.size());
                subs.reserve(subscribers_.size());
                for (auto & sub : subscribers_) {
                    poll_items.push_back({ static_cast<void*>(*sub.socket), 0, ZMQ_POLLIN, 0 });
                    subs.push_back(&sub);
                }
            }

            zmq::poll(poll_items.data(), poll_items.size(), std::chrono::milliseconds(100)); // no lock
            for (size_t i = 0; i < poll_items.size(); ++i) {
                if (poll_items[i].revents & ZMQ_POLLIN) {
                    auto& sub = subs[i];
                    zmq::message_t msg;
                    if(!sub->socket->recv(msg, zmq::recv_flags::none)) {
                        continue;
                    }
                    ByteView view{(uint8_t*)msg.data(), msg.size()};
                    sub->callback(view);
                }
            }
        }
    }

private:

    NodeInfoManager& node_info_mgr_;

    std::vector<Subscriber> subscribers_;
    std::mutex mutex_;
    std::thread poll_thread_;
    bool is_running_ = false;
};

} // namespace lancom
