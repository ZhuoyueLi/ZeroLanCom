#pragma once
#include "nodes/node_info_manager.hpp"
#include "nodes/node_info.hpp"
#include "nodes/multicast.hpp"
#include "sockets/service_manager.hpp"
#include "sockets/subscriber_manager.hpp"
// #include "sockets/zmq_request_helper.hpp"
#include <string>
#include <thread>
#include <chrono>

namespace lancom {

class LanComNode {
public:

static LanComNode& init(const std::string& name, const std::string& ip) {
    std::call_once(flag_, [&]() {
        instance_.reset(new LanComNode(name, ip));
    });
    return *instance_;
}

static LanComNode& instance() {
    if (!instance_) {
        throw std::runtime_error("LanComNode not initialized!");
    }
    return *instance_;
}

LanComNode(const LanComNode&) = delete;
LanComNode& operator=(const LanComNode&) = delete;

LanComNode(const std::string& name,
    const std::string& ip,
    const std::string& group="224.0.0.1",
    int groupPort=7720)
    : localInfo(name, ip),
    mcastSender(group, groupPort, ip),
    mcastReceiver(group, groupPort, ip),
    serviceManager(ip),
    subscriberManager(nodesManager)
    {
        mcastSender.start(localInfo);
        mcastReceiver.start(nodesManager);
        serviceManager.start();
        subscriberManager.start();
    }

    ~LanComNode() {
        stop();
    }

    void stop() {
        running = false;
        mcastSender.stop();
        mcastReceiver.stop();
    }

    void spin() {
        try
        {
            while (running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }
    }

    void sleep(int ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    template <typename HandlerT>
    void registerServiceHandler(const std::string& name, HandlerT handler)
    {
        serviceManager.registerHandler(name, std::function(handler));
        localInfo.registerServices(name, serviceManager.service_port_);

        LOG_INFO("Service {} registered at port {}", name, serviceManager.service_port_);
    }

    template <typename HandlerT, typename ClassT>
    void registerServiceHandler(const std::string& name, HandlerT handler, ClassT* instance)
    {
        serviceManager.registerHandler(name, std::bind(handler, instance));
        localInfo.registerServices(name, serviceManager.service_port_);

        LOG_INFO("Service {} registered at port {}",
                name, serviceManager.service_port_);
    }

    void registerTopic(
        const std::string& topic_name,
        int port)
    {
        localInfo.registerTopic(topic_name, static_cast<uint16_t>(port));
        LOG_INFO("Topic {} registered at port {}", topic_name, port);
    }

    template <typename MessageType>
    void registerSubscriber(
        const std::string& topic_name,
        std::function<void(const MessageType&)> callback)
    {
        subscriberManager.registerTopicSubscriber<MessageType>(topic_name, callback);
    }


    const std::string& GetIP() const {
        return localInfo.nodeInfo.ip;
    }

    NodeInfoManager nodesManager;

    private:
    LocalNodeInfo localInfo;
    
    MulticastSender mcastSender;
    MulticastReceiver mcastReceiver;
    ServiceManager serviceManager;
    SubscriberManager subscriberManager;
    
    bool running = true;
    int startZmqServicePort() { return 7000; }

    static inline std::unique_ptr<LanComNode> instance_;
    static inline std::once_flag flag_;

};

} // namespace lancom