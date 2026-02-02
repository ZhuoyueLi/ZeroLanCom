# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.2] - 2026-02-02

### Changed

- **Include cleanup**: Removed unnecessary includes across the codebase to reduce compilation dependencies and improve build times
  - Removed unused `<iostream>`, `<chrono>`, `<algorithm>`, `<cstring>`, `<thread>` includes from various source files
  - Removed unused `<mutex>`, `<zmq.hpp>`, and internal header includes from `zerolancom_node.hpp`
  - Replaced `std::cout`/`std::cerr` usage with `zlc::info()`/`zlc::error()` logger calls for consistency

### Internal

- Standardized logging usage across all source files (no direct iostream usage)

## [2.0.1] - 2026-01-26

### Added

- **Explicit shutdown API**: Added `zlc::shutdown()` function for graceful node termination
- **Logger lifecycle management**: Added `Logger::shutdown()` and `Logger::isInitialized()` for safe logging during application shutdown
- **Configurable multicast parameters**: Extended `zlc::init()` to accept custom multicast group, port, and group name parameters
  ```cpp
  void init(const std::string &node_name, const std::string &ip_address,
            const std::string &group = "224.0.0.1", int groupPort = 7720,
            const std::string &groupName = "zlc_default_group_name");
  ```

### Changed

- **MessagePack serialization**: Changed from `MSGPACK_DEFINE` to `MSGPACK_DEFINE_MAP` for `SocketInfo` and `NodeInfo` structs, enabling named field serialization for better cross-language compatibility
- **Singleton destruction order**: Fixed destruction order in `ZeroLanComNode::stop()` to properly respect component dependencies (destroy in reverse initialization order)
- **Logger guards**: All logging functions now check `Logger::isInitialized()` before calling spdlog to prevent segfaults during shutdown

### Fixed

- **Critical segfault fix**: Fixed `Singleton::destroy()` method that was not properly destroying managed singletons (used by `ZeroLanComNode`). The bug caused destructors to only run during `__cxa_finalize` when spdlog was already destroyed, leading to segfaults
- **Default argument duplication**: Removed duplicate default arguments from `zerolancom.cpp` implementation (C++ requires defaults only in declarations)

### Removed

- Removed verbose shutdown log messages from `PeriodicTask` and `ThreadPool` to reduce log noise

### Internal

- Updated all test fixtures to call `zlc::shutdown()` in `TearDown()` for proper cleanup
- Improved singleton pattern to correctly handle both managed (`initManaged`) and external (`initExternal`) instances

