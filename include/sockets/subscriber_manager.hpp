#pragma once
#include <unordered_map>
#include <functional>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <zmq.hpp>
#include "utils/logger.hpp"
#include "utils/binary_codec.hpp"
#include "nodes/node_info_manager.hpp"

namespace lancom {

class SubscriberManager {
public:
    SubscriberManager(NodeInfoManager& node_info_mgr)
        : context_(ZmqContext::instance()),
          node_info_mgr_(node_info_mgr) {}

    ~SubscriberManager() {
        stop();
        clearSubscribers();
    }

    // ---------------------------------------------------------------------
    // Register topic-subscriber: automatically discover all publishers
    // ---------------------------------------------------------------------
    void registerTopicSubscriber(
        const std::string& topicName,
        const std::function<void(const ByteView&)>& callback)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        Subscriber sub;
        sub.topicName = topicName;
        sub.callback = callback;
        sub.socket = std::make_unique<zmq::socket_t>(context_, zmq::socket_type::sub);
        sub.socket->set(zmq::sockopt::subscribe, "");

        // initial URL list
        sub.publisherURLs = findTopicURLs(topicName);
        for (auto& url : sub.publisherURLs) {
            sub.socket->connect(url);
            LOG_INFO("[SubscriberManager] '{}' connected to {}", topicName, url);
        }

        subscribers_[topicName] = std::move(sub);
    }

    // ---------------------------------------------------------------------
    // Find all publisher URLs for a topic
    // ---------------------------------------------------------------------
    std::vector<std::string> findTopicURLs(const std::string& topicName) {
        std::vector<std::string> urls;

        auto infos = node_info_mgr_.get_publisher_info(topicName);
        for (auto& t : infos) {
            urls.push_back("tcp://" + t.ip + ":" + std::to_string(t.port));
        }
        return urls;
    }

    // ---------------------------------------------------------------------
    // Called by NodeInfoManager when NodeInfo updated
    // ---------------------------------------------------------------------
    void updateTopicSubscriber(const std::string& topicName) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = subscribers_.find(topicName);
        if (it == subscribers_.end()) return;

        auto newURLs = findTopicURLs(topicName);
        auto& sub = it->second;

        // Compare: if identical — do nothing
        if (newURLs == sub.publisherURLs) return;

        // rebuild socket
        sub.socket->close();
        sub.socket = std::make_unique<zmq::socket_t>(context_, zmq::socket_type::sub);
        sub.socket->set(zmq::sockopt::subscribe, "");

        for (auto& url : newURLs) {
            sub.socket->connect(url);
            LOG_INFO("[SubscriberManager] '{}' reconnected to {}", topicName, url);
        }

        sub.publisherURLs = newURLs;
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

    // ---------------------------------------------------------------------
    // Poll loop
    // ---------------------------------------------------------------------
    void pollLoop() {
        while (is_running_) {
            std::vector<zmq::pollitem_t> poll_items;
            std::vector<std::string> names;

            {
                std::lock_guard<std::mutex> lock(mutex_);
                poll_items.reserve(subscribers_.size());
                names.reserve(subscribers_.size());

                for (auto& kv : subscribers_) {
                    poll_items.push_back({ static_cast<void*>(*kv.second.socket),
                                           0, ZMQ_POLLIN, 0 });
                    names.push_back(kv.first);
                }
            }

            if (poll_items.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                continue;
            }

            zmq::poll(poll_items.data(), poll_items.size(), 50);

            for (size_t i = 0; i < poll_items.size(); ++i) {
                if (poll_items[i].revents & ZMQ_POLLIN) {
                    auto& name = names[i];

                    zmq::message_t msg;
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        subscribers_[name].socket->recv(msg, zmq::recv_flags::none);
                    }

                    ByteView view {
                        static_cast<const uint8_t*>(msg.data()), msg.size()
                    };

                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        subscribers_[name].callback(view);
                    }
                }
            }
        }
    }

private:
    zmq::context_t& context_;
    NodeInfoManager& node_info_mgr_;

    std::unordered_map<std::string, Subscriber> subscribers_;
    std::mutex mutex_;
    std::thread poll_thread_;
    bool is_running_ = false;
};

} // namespace lancom
