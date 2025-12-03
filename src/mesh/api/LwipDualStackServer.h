#pragma once

#include "configuration.h"
#include <WiFi.h>
#include <sys/socket.h>

#if HAS_WIFI

/**
 * Dual-stack TCP server implementation using lwip sockets
 *
 * Provides IPv4 and IPv6 TCP server functionality using raw lwip/POSIX sockets.
 * Maintains API compatibility with Arduino WiFiServer for integration with
 * the existing APIServerPort template.
 *
 * When IPv6 is enabled (config.network.ipv6_enabled), creates two separate
 * listening sockets:
 * - IPv4: binds to 0.0.0.0:port
 * - IPv6: binds to [::]:port with IPV6_V6ONLY=1
 *
 * Both sockets are set to non-blocking mode for compatibility with the
 * FreeRTOS runOnce() polling pattern.
 */
class LwipDualStackServer
{
  private:
    int sockfd_v4;       // IPv4 listening socket (-1 if not created)
    int sockfd_v6;       // IPv6 listening socket (-1 if not created)
    uint16_t serverPort; // Port number to listen on
    bool started;        // Whether begin() has been called

    /**
     * Create and configure a listening socket
     * @param addr_family AF_INET or AF_INET6
     * @param addr Pointer to sockaddr_in or sockaddr_in6
     * @param addr_len Size of address structure
     * @return Socket fd on success, -1 on failure
     */
    int createListenSocket(int addr_family, void *addr, socklen_t addr_len);

  public:
    /**
     * Constructor
     * @param port TCP port number to listen on (default: 4403)
     */
    explicit LwipDualStackServer(int port);

    /**
     * Destructor - closes sockets if still open
     */
    ~LwipDualStackServer();

    /**
     * Start listening on IPv4 and optionally IPv6 sockets
     * Creates, binds, and sets up listening sockets based on configuration.
     * If config.network.ipv6_enabled is true, creates both IPv4 and IPv6 sockets.
     * Otherwise, creates only IPv4 socket.
     */
    void begin();

    /**
     * Accept a pending connection from either IPv4 or IPv6 socket
     * Non-blocking: returns empty WiFiClient if no connections pending.
     * Checks IPv4 socket first, then IPv6 socket if enabled.
     * @return WiFiClient wrapping the accepted connection, or empty if none available
     */
    WiFiClient accept();

    /**
     * Compatibility method for older Arduino API
     * Calls accept() internally
     * @return WiFiClient wrapping the accepted connection, or empty if none available
     */
    WiFiClient available() { return accept(); }

    /**
     * Stop server and close all sockets
     */
    void end();

    /**
     * Get the port number this server is listening on
     * @return Port number
     */
    uint16_t port() const { return serverPort; }
};

#endif // HAS_WIFI
