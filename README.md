# ZeroLanCom

ZeroLanCom is a lightweight communication framework built on top of **ZeroMQ** and **MessagePack**, providing:

- **Publish / Subscribe messaging**
- **Synchronous Service calls (RPC)**
- **Automatic message serialization**
- **Dynamic node discovery**
- **Simple C++ API**

ZeroLanCom is header-only on the core logic and extremely easy to integrate into existing C++ applications.

## ✨ Features

- 📨 Topic-based Pub/Sub
- 🛎️ Service (RPC) with automatic encoding / decoding
- 🧩 MessagePack serialization
- ⚙️ ZeroMQ as transport layer
- 🧭 Node information sharing and discovery
- 🧹 Header-only core (no separate compilation needed)
- 📦 Simple API with clean async flow

## 🔧 Build & Install

### Dependencies

- ZeroMQ
- spdlog
- msgpack-c++

Install on Ubuntu:

```bash
sudo apt install libzmq3-dev libspdlog-dev
```

Clone and build:

```bash
mkdir build && cd build
cmake ..
make install
```

## 🚀 Quick Start

### Initialization

```cpp
#include <zerolancom/zerolancom.hpp>

int main() {
    // Basic initialization with node name and local IP
    zlc::init("my_node", "192.168.1.100");
    
    // ... your code here ...
    
    zlc::shutdown();
    return 0;
}
```

### Configuration Parameters

The `zlc::init()` function accepts the following parameters:

```cpp
void init(
    const std::string &node_name,      // Unique name for this node
    const std::string &ip_address,     // Local IP address to bind to
    const std::string &group = "224.0.0.1",           // Multicast group IP
    int groupPort = 7720,                             // Multicast port
    const std::string &groupName = "zlc_default_group_name"  // Logical group name
);
```

| Parameter | Description | Default |
|-----------|-------------|---------|
| `node_name` | Unique identifier for this node in the network | (required) |
| `ip_address` | Local IP address for binding sockets (e.g., `"192.168.1.100"`) | (required) |
| `group` | Multicast group IP address for node discovery | `"224.0.0.1"` |
| `groupPort` | UDP port for multicast heartbeat messages | `7720` |
| `groupName` | Logical group name - only nodes with the same group name can discover each other | `"zlc_default_group_name"` |

### Understanding the Parameters

**Local IP (`ip_address`)**
- The IP address of the network interface to use
- Must be a valid IP on your machine (not `0.0.0.0` or `127.0.0.1` for multi-node setups)
- Example: `"192.168.1.100"`, `"10.0.0.5"`

**Multicast Group IP (`group`)**
- Must be in the multicast range `224.0.0.0` - `239.255.255.255`
- All nodes that need to discover each other should use the same multicast group
- Default `224.0.0.1` works for most local network setups

**Multicast Port (`groupPort`)**
- UDP port for sending/receiving heartbeat messages
- All nodes in the same network must use the same port
- Default `7720` is typically available

**Group Name (`groupName`)**
- Logical filter for node discovery
- Only nodes with **matching group names** will discover and communicate with each other
- Useful for running multiple isolated ZeroLanCom networks on the same physical network
- Example: Use `"production"` and `"development"` to separate environments

### Example: Custom Configuration

```cpp
// Production environment
zlc::init("sensor_node_1", "192.168.1.50", "224.0.1.100", 8800, "production");

// Development environment (isolated from production)
zlc::init("test_node", "192.168.1.50", "224.0.1.100", 8800, "development");
```