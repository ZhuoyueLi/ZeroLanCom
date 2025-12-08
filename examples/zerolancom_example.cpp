#include "zerolancom_node.hpp"
#include "utils/logger.hpp"
#include "sockets/publisher.hpp"
#include "sockets/client.hpp"

void topicCallback(const std::string& msg) {
    LOG_INFO("Received message on subscribed topic: {}", msg);
}

std::string serviceHandler(const std::string& request) {
    LOG_INFO("Service request received.");
    return std::string("Echo: ") + request;
}

int main() {
    //initialize logger
    zlc::Logger::init(false); //true to enable file logging
    zlc::Logger::setLevel(zlc::LogLevel::INFO);
    zlc::ZeroLanComNode& node = zlc::ZeroLanComNode::init("TestNode", "127.0.0.1");
    node.registerServiceHandler("EchoService", serviceHandler);
    node.registerSubscriber<std::string>("TestTopic", topicCallback);
    zlc::Publisher<std::string> publisher("TestTopic");
    node.registerServiceHandler("EchoService2", serviceHandler);
    zlc::Client::waitForService("EchoService");
    std::string response = "";
    zlc::Client::request<std::string, std::string>("EchoService", "Hello Service", response);
    try
    {
        while (true) {
            publisher.publish("Hello, ZeroLanCom!");
            LOG_INFO("Published message to TestTopic");
            node.sleep(1000);
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    
    return 0;
}