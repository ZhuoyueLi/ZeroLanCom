#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <fmt/format.h>
#include <zmq.hpp>

#include "zerolancom/nodes/node_info.hpp"
#include "zerolancom/nodes/node_info_manager.hpp"
#include "zerolancom/serialization/serializer.hpp"
#include "zerolancom/utils/logger.hpp"
#include "zerolancom/utils/thread_pool.hpp"
#include "zerolancom/utils/zmq_utils.hpp"

namespace zlc
{

/**
 * @brief SubscriberManager manages topic subscriptions and message dispatch.
 *
 * Design notes:
 * - Automatically discovers publishers via NodeInfoManager callbacks.
 * - Uses one SUB socket per topic.
 * - Callbacks are dispatched via ThreadPool to avoid blocking the poll loop.
 * - Template subscription API must remain header-only.
 */
class SubscriberManager : public Singleton<SubscriberManager>
{
public:
  explicit SubscriberManager();
  ~SubscriberManager();

  /**
   * @brief Register a subscriber callback for a topic.
   *
   * Requirements:
   * - MessageType must be decodable via decode().
   * - Callback is executed in the polling thread.
   */
  template <typename MessageType>
  void registerTopicSubscriber(const std::string &topicName,
                               void (*callback)(const MessageType &))
  {
    _registerTopicSubscriber(topicName,
                             [callback](const ByteView &view)
                             {
                               MessageType msg;
                               decode(view, msg);
                               callback(msg);
                             });
  }

  template <typename MessageType, typename ClassT>
  void registerTopicSubscriber(const std::string &topicName,
                               void (ClassT::*callback)(const MessageType &),
                               ClassT *instance)
  {
    _registerTopicSubscriber(topicName,
                             [instance, callback](const ByteView &view)
                             {
                               MessageType msg;
                               decode(view, msg);
                               (instance->*callback)(msg);
                             });
  }

  // Start polling thread
  void start();

  // Stop polling thread
  void stop();

  // Called by NodeInfoManager when a node announces new topics
  void updateTopicSubscriber(const NodeInfo &nodeInfo);

  // Called by NodeInfoManager when a node is removed
  void removeTopicSubscriber(const NodeInfo &nodeInfo);

private:
  // Find all publisher endpoints for a topic
  std::vector<std::string> findTopicURLs(const std::string &topicName);

  // Poll once for incoming messages
  void pollOnce();

private:
  struct Subscriber
  {
    std::string topicName;
    std::vector<std::string> publisherURLs;
    std::function<void(const ByteView &)> callback;
    ZMQSocket *socket;
  };

private:
  std::vector<Subscriber> subscribers_;
  std::mutex mutex_;

  std::thread thread_;
  std::atomic<bool> running_{false};

  void run();

  void _registerTopicSubscriber(const std::string &topicName,
                                const std::function<void(const ByteView &)> &callback);
};

} // namespace zlc
