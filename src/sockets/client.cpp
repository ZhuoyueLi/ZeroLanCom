#include "zerolancom/sockets/client.hpp"
#include "zerolancom/nodes/node_info_manager.hpp"

namespace zlc
{

void Client::sendRequest(const std::string &service_name, const ByteView &payload,
                         ZMQSocket &socket)
{
  // Send service name frame
  socket.send(zmq::buffer(service_name), zmq::send_flags::sndmore);

  // Send payload frame
  socket.send(zmq::buffer(payload.data, payload.size), zmq::send_flags::none);

  zlc::info("[Client] Sent request to service '{}'", service_name);
}

void Client::receiveResponse(ZMQSocket &socket, zmq::message_t &payloadMsg,
                             const std::string &service_name)
{
  zmq::message_t statusMsg;

  if (!socket.recv(statusMsg, zmq::recv_flags::none))
  {
    zlc::error("Timeout waiting for response from service {}", service_name);
    return;
  }

  if (!statusMsg.more())
  {
    zlc::error("No payload frame received for service response from {}", service_name);
    return;
  }

  if (!socket.recv(payloadMsg, zmq::recv_flags::none))
  {
    zlc::error("Timeout waiting for payload from service {}", service_name);
    return;
  }

  if (payloadMsg.more())
  {
    zlc::error("More frames received than expected from service {}", service_name);
  }
}

} // namespace zlc
