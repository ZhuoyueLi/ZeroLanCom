#pragma once
#include <iostream>

class Logger {
public:
  enum Level { DEBUG, INFO, WARNING, ERROR, CRITICAL };
  static void log(Level level, const std::string &message) {
    const char *level_str[] = {"DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"};
    std::cout << "[" << level_str[level] << "] " << message << std::endl;
  }
  static void debug(const std::string &message) { log(DEBUG, message); }
  static void info(const std::string &message) { log(INFO, message); }
  static void warning(const std::string &message) { log(WARNING, message); }
};