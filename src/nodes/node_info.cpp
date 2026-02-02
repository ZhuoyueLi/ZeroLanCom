#include "zerolancom/nodes/node_info.hpp"

#include "zerolancom/utils/logger.hpp"

namespace zlc
{

/* ================= NodeInfo ================= */

void NodeInfo::printNodeInfo() const
{
  zlc::info("-------------------------------------------------------------");
  zlc::info("NodeID: {}", nodeID);
  zlc::info("InfoID: {}", infoID);
  zlc::info("Name: {}", name);
  zlc::info("IP: {}", ip);
  zlc::info("Topics:");
  for (const auto &t : topics)
  {
    zlc::info("  - {}:{}", t.name, t.port);
  }
  zlc::info("Services:");
  for (const auto &s : services)
  {
    zlc::info("  - {}:{}", s.name, s.port);
  }
}

} // namespace zlc
