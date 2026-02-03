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
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
      poll_items.reserve(subscribers_.size());
      subs.reserve(subscribers_.size());

      for (auto &sub : subscribers_)
      {
        poll_items.push_back({sub.socket->handle(), 0, ZMQ_POLLIN, 0});
        subs.push_back(&sub);
      }
    }

    zmq::poll(poll_items.data(), poll_items.size(), std::chrono::milliseconds(10));

    for (size_t i = 0; i < poll_items.size(); ++i)
    {
      if (poll_items[i].revents & ZMQ_POLLIN)
      {
        zmq::message_t msg;
        if (!subs[i]->socket->recv(msg, zmq::recv_flags::none))
        {
          continue;
        }

        // Copy message data for ThreadPool dispatch
        Bytes data(static_cast<const uint8_t *>(msg.data()),
                   static_cast<const uint8_t *>(msg.data()) + msg.size());
        auto callback = subs[i]->callback;

        // Dispatch callback to ThreadPool to avoid blocking poll loop
        ThreadPool::instance().enqueue(
            [callback, data = std::move(data)]()
            {
              ByteView view{data.data(), data.size()};
              callback(view);
            });
      }
    }
  }
  catch (const zmq::error_t &e)
  {
    if (e.num() == ETERM)
    {
      zlc::info("[SubscriberManager] Context terminated during poll");
      return;
    }
    zlc::error("[SubscriberManager] ZMQ error: {}", e.what());
  }
  catch (const std::exception &e)
  {
    zlc::info("[SubscriberManager] Poll exception: {}", e.what());
  }
}

} // namespace zlc
