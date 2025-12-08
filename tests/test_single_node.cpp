#include <gtest/gtest.h>

#include "lancom_node.hpp"
#include "utils/logger.hpp"
#include "sockets/lancom_publisher.hpp"
#include "sockets/lancom_client.hpp"

#include "test_utils.hpp"

TEST(LanComTest, BasicPubSubAndService)
{
    using namespace zlc;

    // Store callback result
    std::string received_topic_msg;
    std::string received_service_resp;

    // ---- Create node ----
    ZeroLanComNode& node = ZeroLanComNode::init("TestNode", "127.0.0.1");

    // ---- Register Topic Callback ----
    node.registerSubscriber<std::string>("TestTopic", topicCallback);

    // ---- Register Service Handler ----
    node.registerServiceHandler("EchoService", serviceHandler);

    // ---- Publisher ----
    Publisher<std::string> publisher("TestTopic");

    // ---- Service Call ----
    Client::waitForService("EchoService");
    Client::request<const std::string&, std::string>(
        "EchoService", "Hello Service", received_service_resp
    );

    // ---- Publish message ----
    publisher.publish("Hello, ZeroLanCom!");

    // Allow some time for async message passing
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // ---- Check service response ----
    EXPECT_EQ(received_service_resp, "Hello Service");

    // ---- Check topic callback ----
    EXPECT_EQ(received_topic_msg, "Hello, ZeroLanCom!");
}
