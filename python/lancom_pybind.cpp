#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>

#include "lancom_node.hpp"
#include "utils/logger.hpp"
#include "sockets/lancom_publisher.hpp"
#include "sockets/lancom_client.hpp"

namespace py = pybind11;

using zlc::ZeroLanComNode;
using zlc::Publisher;
using zlc::Client;
using zlc::Logger;
using zlc::LogLevel;

// Logger functions
void init_logger(bool to_file) {
    Logger::init(to_file);
}

void set_log_level(LogLevel level) {
    Logger::setLevel(level);
}

PYBIND11_MODULE(lancom_py, m) {
    m.doc() = "Python bindings for LanCom";

    py::enum_<LogLevel>(m, "LogLevel")
        .value("TRACE", LogLevel::TRACE)
        .value("INFO",  LogLevel::INFO)
        .value("WARN",  LogLevel::WARN)
        .value("ERROR", LogLevel::ERROR)
        .value("FATAL", LogLevel::FATAL)
        .export_values();

    // Logger interface
    m.def("init_logger", &init_logger, py::arg("to_file") = false);
    m.def("set_log_level", &set_log_level);

    // ZeroLanComNode
    py::class_<ZeroLanComNode>(m, "ZeroLanComNode")
        // C++ static method mapped to Python static method
        .def_static(
            "init",
            &ZeroLanComNode::init,
            py::arg("name"), py::arg("ip"),
            py::return_value_policy::reference,  // Return reference, consistent with C++
            "Initialize and get global ZeroLanComNode instance"
        )
        .def(
            "sleep",
            &ZeroLanComNode::sleep,
            py::arg("ms"),
            "Sleep for given milliseconds"
        )
        // Convert Python function to C++ callback
        .def(
            "register_service_handler",
            [](ZeroLanComNode& self,
               const std::string& service_name,
               py::function handler) {
                // Wrap Python callback into std::function<std::string(const std::string&)>
                self.registerServiceHandler(
                    service_name,
                    [handler](const std::string& request) {
                        py::gil_scoped_acquire gil;  // Acquire GIL before calling Python
                        py::object result = handler(request);
                        return result.cast<std::string>();
                    }
                );
            },
            py::arg("service_name"),
            py::arg("handler"),
            "Register a Python function as service handler"
        )
        .def(
            "register_subscriber",
            [](ZeroLanComNode& self,
               const std::string& topic,
               py::function callback) {
                self.registerSubscriber<std::string>(
                    topic,
                    [callback](const std::string& msg) {
                        py::gil_scoped_acquire gil;
                        callback(msg);
                    }
                );
            },
            py::arg("topic"),
            py::arg("callback"),
            "Register a Python function as topic subscriber"
        );

    // Publisher: only bind std::string version for now
    py::class_<Publisher<std::string>>(m, "StringPublisher")
        .def(py::init<const std::string&>(),
             py::arg("topic"))
        .def("publish",
             &Publisher<std::string>::publish,
             py::arg("msg"));

    // Client: wait for service & request
    py::class_<Client>(m, "Client")
        .def_static(
            "wait_for_service",
            &Client::waitForService,
            py::arg("service_name"),
            "Block until service is available"
        )
        .def_static(
            "request",
            [](const std::string& service_name,
               const std::string& request) {
                std::string response;
                Client::request<std::string, std::string>(
                    service_name, request, response
                );
                return response;
            },
            py::arg("service_name"),
            py::arg("request"),
            "Send request and return response as string"
        );
}
