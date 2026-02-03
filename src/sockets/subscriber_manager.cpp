#include "zerolancom/sockets/subscriber_manager.hpp"

#include <algorithm>
#include <chrono>

namespace zlc
{

SubscriberManager::SubscriberManager()
{
  // Subscribe to node/topic updates
  NodeInfoManager::instance().node_update_event.subscribe(std::bind(
      &SubscriberManager::updateTopicSubscriber, this, std::placeholders::_1));

  // Subscribe to node removal events
  NodeInfoManager::instance().node_remove_event.subscribe(std::bind(
      &SubscriberManager::removeTopicSubscriber, this, std::placeholders::_1));
}

SubscriberManager::~SubscriberManager()
{
  stop();
}

void SubscriberManager::start()
{
  running_ = true;
  thread_ = std::thread([this]() { this->run(); });
}

void SubscriberManager::stop()
{
  if (running_)
  {
    running_ = false;
    if (thread_.joinable())
    {
      thread_.join();
    }
  }
}

void SubscriberManager::run()
{
  while (running_)
  {
    pollOnce();
  }
}

void SubscriberManager::_registerTopicSubscriber(
    const std::string &topicName, const std::function<void(const ByteView &)> &callback)
{
  std::lock_guard<std::mutex> lock(mutex_);

  Subscriber sub;
  sub.topicName = topicName;
  sub.callback = callback;

  sub.socket = ZMQContext::createSocket(zmq::socket_type::sub);
  sub.socket->set(zmq::sockopt::subscribe, "");
  auto urls = findTopicURLs(topicName);
  for (const auto &url : urls)
  {
    sub.socket->connect(url);
    zlc::info("[SubscriberManager] '{}' connected to {}", topicName, url);
    sub.publisherURLs.push_back(url);
  }
  subscribers_.push_back(std::move(sub));
}

std::vector<std::string> SubscriberManager::findTopicURLs(const std::string &topicName)
{
  std::vector<std::string> urls;

  auto infos = NodeInfoManager::instance().getPublisherInfo(topicName);

  for (const auto &t : infos)
  {
    urls.push_back(fmt::format("tcp://{}:{}", t.ip, t.port));
  }

  return urls;
}

void SubscriberManager::updateTopicSubscriber(const NodeInfo &nodeInfo)
{
  std::lock_guard<std::mutex> lock(mutex_);

  for (const auto &topic : nodeInfo.topics)
  {
    for (auto &sub : subscribers_)
    {
      if (sub.topicName != topic.name)
        continue;

      std::string url = fmt::format("tcp://{}:{}", topic.ip, topic.port);

      if (std::find(sub.publisherURLs.begin(), sub.publisherURLs.end(), url) !=
          sub.publisherURLs.end())
      {
        continue; // already connected
      }

      sub.socket->connect(url);
      sub.publisherURLs.push_back(url);

      zlc::info("[SubscriberManager] '{}' connected to {}", topic.name, url);
    }
  }
}

void SubscriberManager::removeTopicSubscriber(const NodeInfo &nodeInfo)
{
  std::lock_guard<std::mutex> lock(mutex_);

  for (const auto &topic : nodeInfo.topics)
  {
    for (auto &sub : subscribers_)
    {
      if (sub.topicName != topic.name)
        continue;

      std::string url = fmt::format("tcp://{}:{}", topic.ip, topic.port);

      auto it = std::find(sub.publisherURLs.begin(), sub.publisherURLs.end(), url);
      if (it == sub.publisherURLs.end())
      {
        continue; // not connected to this publisher
      }

      sub.socket->disconnect(url);
      sub.publisherURLs.erase(it);

      zlc::info("[SubscriberManager] '{}' disconnected from {}", topic.name, url);
    }
  }
}

void SubscriberManager::pollOnce()
{
  try
  {
    std::vector<zmq::pollitem_t> poll_items;
    std::vector<Subscriber *> subs;

    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (subscribers_.empty()) return;
      
      poll_items.reserve(subscribers_.size());
      subs.reserve(subscribers_.size());

      for (auto &sub : subscribers_)
      {
        poll_items.push_back({sub.socket->handle(), 0, ZMQ_POLLIN, 0});
        subs.push_back(&sub);
      }
    }

    int rc = zmq::poll(poll_items.data(), poll_items.size(), std::chrono::milliseconds(100));
    if (rc <= 0) return;

    for (size_t i = 0; i < poll_items.size(); ++i)
    {
      if (poll_items[i].revents & ZMQ_POLLIN)
      {
        zmq::message_t last_msg;
        zmq::message_t tmp_msg;
        bool has_data = false;

        while (subs[i]->socket->recv(tmp_msg, zmq::recv_flags::dontwait))
        {
          last_msg = std::move(tmp_msg);
          has_data = true;
        }

        if (has_data)
        {
          ByteView view{static_cast<const uint8_t*>(last_msg.data()), last_msg.size()};
          
          if (subs[i]->callback) {
            subs[i]->callback(view);
          }
        }
      }
    }
  }
  catch (const zmq::error_t &e)
  {
    if (e.num() == ETERM) return;
    zlc::error("[SubscriberManager] ZMQ error: {}", e.what());
  }
  catch (const std::exception &e)
  {
    zlc::error("[SubscriberManager] Exception: {}", e.what());
  }
}
} // namespace zlc
