#pragma once
#include <string>
#include <vector>

struct NodeAddress {
  std::string ip;
  int port;
};

struct NodeInfo {
  std::string name;
  std::string nodeID;
  NodeAddress addr;
  std::string type;
  int servicePort;
  int topicPort;
  std::vector<std::string> serviceList;
  std::vector<std::string> topicList;
};
