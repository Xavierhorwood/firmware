#include "configuration.h"
#include <Arduino.h>

#if HAS_WIFI
#include "WiFiServerAPI.h"
#include "NodeDB.h"

// Use void* to allow polymorphic storage of either WiFiServerPort or WiFiServerPortDual
static void *apiPort = nullptr;
static bool usingDualStack = false;

void initApiServer(int port)
{
    if (apiPort) {
        LOG_WARN("API server already initialized");
        return;
    }

    // Use dual-stack server when IPv6 is enabled, otherwise use IPv4-only
    if (config.network.ipv6_enabled) {
        LOG_INFO("Starting dual-stack (IPv4+IPv6) API server on port %d", port);
        auto *dualPort = new WiFiServerPortDual(port);
        dualPort->init();
        apiPort = dualPort;
        usingDualStack = true;
    } else {
        LOG_INFO("Starting IPv4-only API server on port %d", port);
        auto *v4Port = new WiFiServerPort(port);
        v4Port->init();
        apiPort = v4Port;
        usingDualStack = false;
    }
}

void deInitApiServer()
{
    if (apiPort) {
        if (usingDualStack) {
            delete static_cast<WiFiServerPortDual *>(apiPort);
        } else {
            delete static_cast<WiFiServerPort *>(apiPort);
        }
        apiPort = nullptr;
        usingDualStack = false;
    }
}

WiFiServerAPI::WiFiServerAPI(WiFiClient &_client) : ServerAPI(_client)
{
    api_type = TYPE_WIFI;
    LOG_INFO("Incoming wifi connection");
}

WiFiServerPort::WiFiServerPort(int port) : APIServerPort(port) {}

WiFiServerPortDual::WiFiServerPortDual(int port) : APIServerPort(port) {}

#endif