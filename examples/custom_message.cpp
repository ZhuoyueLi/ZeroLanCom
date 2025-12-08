#include <msgpack.hpp>
#include "zerolancom_node.hpp"
#include "utils/logger.hpp"
#include "sockets/publisher.hpp"
#include "sockets/client.hpp"

void topicCallback(const std::string& msg) {
    LOG_INFO("Received message on subscribed topic: {}", msg);
}

struct CustomMessage
{
    int count;
    std::string name;
    std::vector<float> data;
    MSGPACK_DEFINE_MAP(count, name, data);
};

int main() {
    //initialize logger
    zlc::Logger::init(false); //true to enable file logging
    zlc::Logger::setLevel(zlc::LogLevel::INFO);
    zlc::ZeroLanComNode& node = zlc::ZeroLanComNode::init("CustomMessageNode", "127.0.0.1");
    zlc::Publisher<CustomMessage> publisher("CustomMessage");
    try
    {
        int counter = 0;
        while (true) {
            publisher.publish(CustomMessage{counter, "CustomMessage", {1.0f, 2.0f, 3.0f}});
            counter++;
            LOG_INFO("Published message to CustomMessage");
            node.sleep(1000);
        }
    }   
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    
    return 0;
}