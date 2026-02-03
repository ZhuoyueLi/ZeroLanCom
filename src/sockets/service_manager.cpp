#include "zerolancom/sockets/service_manager.hpp"

#include <chrono>

#include "zerolancom/utils/exception.hpp"

namespace zlc
{

ServiceManager::ServiceManager(const std::string &ip)
{
  res_socket_ = ZMQContext::createSocket(zmq::socket_type::rep);
  res_socket_->set(zmq::sockopt::rcvtimeo, SOCKET_TIMEOUT_MS);
  res_socket_->bind("tcp://" + ip + ":0");
  service_port = getBoundPort(*res_socket_);

  zlc::info("[ServiceManager] ServiceManager bound to port {}", service_port);
}

ServiceManager::~ServiceManager()
{
  stop();
}

void ServiceManager::start()
{
  running_ = true;
  thread_ = std::thread([this]() { this->run(); });
}

void ServiceManager::stop()
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

void ServiceManager::run()
{
  while (running_)
  {
    pollOnce();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void ServiceManager::handleRequest(const std::string &service_name,
                                   const ByteView &payload, Response &response)
{
  auto it = handlers_.find(service_name);

  zlc::info("[ServiceManager] Handling request for service '{}'", service_name);

  if (it == handlers_.end())
  {
    response.code = ResponseStatus::NOSERVICE;
    return;
  }

  response.code = ResponseStatus::SUCCESS;

  try
  {
    response.payload = it->second(payload);
  }
  catch (const DecodeException &e)
  {
    zlc::error("[ServiceManager] ZMQ error while handling service '{}': {}",
               service_name, e.what());
    response.code = ResponseStatus::INVALID_RESPONSE;
  }
  catch (const EncodeException &e)
  {
    zlc::error("[ServiceManager] ZMQ error while handling service '{}': {}",
               service_name, e.what());
    response.code = ResponseStatus::INVALID_REQUEST;
  }
  catch (const std::exception &e)
  {
    zlc::error("[ServiceManager] Exception while handling service '{}': {}",
               service_name, e.what());
    response.code = ResponseStatus::SERVICE_FAIL;
  }
}

void ServiceManager::clearHandlers()
{
  handlers_.clear();
}

void ServiceManager::removeHandler(const std::string &name)
{
  handlers_.erase(name);
}

void ServiceManager::pollOnce()
{
  try
  {
    zmq::message_t service_name_msg;

    if (!res_socket_->recv(service_name_msg, zmq::recv_flags::none))
    {
      return;
    }

    std::string service_name = decodeServiceHeader(
        ByteView{static_cast<const uint8_t *>(service_name_msg.data()),
                 service_name_msg.size()});

    if (!service_name_msg.more())
    {
      zlc::warn("[ServiceManager] Missing payload frame");
      return;
    }

    zmq::message_t payload_msg;
    if (!res_socket_->recv(payload_msg, zmq::recv_flags::none))
    {
      return;
    }

    ByteView payload{static_cast<const uint8_t *>(payload_msg.data()),
                     payload_msg.size()};

    if (payload_msg.more())
    {
      zlc::warn("[ServiceManager] Extra frames received");
    }

    Response response;
    handleRequest(service_name, payload, response);

    // Send response frames
    res_socket_->send(zmq::buffer(response.code), zmq::send_flags::sndmore);
    res_socket_->send(zmq::buffer(response.payload), zmq::send_flags::none);
  }
  catch (const zmq::error_t &e)
  {
    if (e.num() == ETERM)
    {
      zlc::info("[ServiceManager] Context terminated during poll");
      return;
    }
    zlc::error("[ServiceManager] ZMQ error: {}", e.what());
  }
}

} // namespace zlc
