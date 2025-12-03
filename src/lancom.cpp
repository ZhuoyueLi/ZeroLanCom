#include "lancom_node.hpp"
#include "utils/logger.hpp"
#include "sockets/publisher_hpp.hpp"

class TestClass {
public:
    void memberFunction() {
        LOG_INFO("TestClass member function called.");
    }
};


int main() {
    //initialize logger
    lancom::Logger::init(false); //true to enable file logging
    lancom::Logger::setLevel(lancom::LogLevel::INFO);
    lancom::LanComNode& node = lancom::LanComNode::init("TestNode", "127.0.0.1");
    node.registerServiceHandler(
        "EchoService",
        [](){
            LOG_INFO("EchoService received request");
        }
    );
    lancom::Publisher<std::string> publisher("TestTopic");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    node.registerServiceHandler(
        "EchoService2",
        [](){
            LOG_INFO("EchoService received request");
        }
    );
    node.spin();
    return 0;
}