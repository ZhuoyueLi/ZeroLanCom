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
make
```