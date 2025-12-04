#include "lancom_node.hpp"
#include "utils/logger.hpp"
#include "sockets/lancom_publisher.hpp"
#include "sockets/lancom_client.hpp"

void topicCallback(const std::string& msg) {
    LOG_INFO("Received message on subscribed topic: {}", msg);
}

std::string serviceHandler(const std::string& request) {
    LOG_INFO("Service request received.");
    return request; // Echo the received request
}

int main() {
    //initialize logger
    lancom::Logger::init(false); //true to enable file logging
    lancom::Logger::setLevel(lancom::LogLevel::INFO);
    lancom::LanComNode& node = lancom::LanComNode::init("TestNode", "127.0.0.1");
    node.registerServiceHandler("EchoService", serviceHandler);
    node.registerSubscriber<std::string>("TestTopic", topicCallback);
    lancom::LanComPublisher<std::string> publisher("TestTopic");
    node.registerServiceHandler("EchoService2", serviceHandler);
    lancom::LanComClient::waitForService("EchoService");
    lancom::LanComClient::request<std::string, std::string>("EchoService", "Hello Service");
    try
    {
        while (true) {
            publisher.publish("Hello, LanCom!");
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