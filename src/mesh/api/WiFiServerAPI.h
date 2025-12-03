#pragma once

#include "ServerAPI.h"
#include "LwipDualStackServer.h"
#include <WiFi.h>

#if HAS_ETHERNET && defined(USE_WS5500)
#include <ETHClass2.h>
#define ETH ETH2
#endif // HAS_ETHERNET

/**
 * Provides both debug printing and, if the client starts sending protobufs to us, switches to send/receive protobufs
 * (and starts dropping debug printing - FIXME, eventually those prints should be encapsulated in protobufs).
 */
class WiFiServerAPI : public ServerAPI<WiFiClient>
{
  public:
    explicit WiFiServerAPI(WiFiClient &_client);
};

/**
 * Listens for incoming connections and does accepts and creates instances of WiFiServerAPI as needed
 */
class WiFiServerPort : public APIServerPort<WiFiServerAPI, WiFiServer>
{
  public:
    explicit WiFiServerPort(int port);
};

/**
 * Dual-stack IPv4+IPv6 TCP API server port
 * Uses lwip sockets directly to support both IPv4 and IPv6 simultaneously.
 *
 * When config.network.ipv6_enabled is true:
 * - Listens on IPv4 0.0.0.0:port
 * - Listens on IPv6 [::]:port
 *
 * When config.network.ipv6_enabled is false:
 * - Falls back to WiFiServerPort (IPv4 only)
 */
class WiFiServerPortDual : public APIServerPort<WiFiServerAPI, LwipDualStackServer>
{
  public:
    explicit WiFiServerPortDual(int port);
};

void initApiServer(int port = SERVER_API_DEFAULT_PORT);
void deInitApiServer();