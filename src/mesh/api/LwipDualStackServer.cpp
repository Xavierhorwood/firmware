#include "configuration.h"

#if HAS_WIFI
#include "LwipDualStackServer.h"
#include "NodeDB.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <cstring>

LwipDualStackServer::LwipDualStackServer(int port) : sockfd_v4(-1), sockfd_v6(-1), serverPort(port), started(false) {}

LwipDualStackServer::~LwipDualStackServer()
{
    end();
}

int LwipDualStackServer::createListenSocket(int addr_family, void *addr, socklen_t addr_len)
{
    // Create socket
    int sockfd = socket(addr_family, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LOG_ERROR("Failed to create %s socket: errno=%d", addr_family == AF_INET ? "IPv4" : "IPv6", errno);
        return -1;
    }

    // Set SO_REUSEADDR to allow rapid restart
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_WARN("setsockopt SO_REUSEADDR failed: errno=%d", errno);
    }

    // For IPv6, set IPV6_V6ONLY to prevent dual-stack behavior on single socket
    if (addr_family == AF_INET6) {
        opt = 1;
        if (setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) < 0) {
            LOG_WARN("setsockopt IPV6_V6ONLY failed: errno=%d", errno);
        }
    }

    // Bind to address
    if (bind(sockfd, (struct sockaddr *)addr, addr_len) < 0) {
        LOG_ERROR("Failed to bind %s socket to port %d: errno=%d", addr_family == AF_INET ? "IPv4" : "IPv6", serverPort,
                  errno);
        close(sockfd);
        return -1;
    }

    // Start listening (backlog=1 matches existing WiFiServer behavior)
    if (listen(sockfd, 1) < 0) {
        LOG_ERROR("Failed to listen on %s socket: errno=%d", addr_family == AF_INET ? "IPv4" : "IPv6", errno);
        close(sockfd);
        return -1;
    }

    // Set non-blocking mode for FreeRTOS compatibility
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags >= 0) {
        if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
            LOG_WARN("Failed to set O_NONBLOCK on %s socket: errno=%d", addr_family == AF_INET ? "IPv4" : "IPv6",
                     errno);
        }
    }

    return sockfd;
}

void LwipDualStackServer::begin()
{
    if (started) {
        LOG_WARN("LwipDualStackServer already started");
        return;
    }

    // Create IPv4 listening socket
    struct sockaddr_in addr_v4;
    memset(&addr_v4, 0, sizeof(addr_v4));
    addr_v4.sin_family = AF_INET;
    addr_v4.sin_addr.s_addr = INADDR_ANY; // 0.0.0.0
    addr_v4.sin_port = htons(serverPort);

    sockfd_v4 = createListenSocket(AF_INET, &addr_v4, sizeof(addr_v4));
    if (sockfd_v4 >= 0) {
        LOG_INFO("API server listening on IPv4 0.0.0.0:%d", serverPort);
    } else {
        LOG_ERROR("Failed to start IPv4 API server");
    }

    // Create IPv6 listening socket if enabled
    if (config.network.ipv6_enabled) {
        struct sockaddr_in6 addr_v6;
        memset(&addr_v6, 0, sizeof(addr_v6));
        addr_v6.sin6_family = AF_INET6;
        addr_v6.sin6_addr = in6addr_any; // [::]
        addr_v6.sin6_port = htons(serverPort);

        sockfd_v6 = createListenSocket(AF_INET6, &addr_v6, sizeof(addr_v6));
        if (sockfd_v6 >= 0) {
            LOG_INFO("API server listening on IPv6 [::]:%d", serverPort);
        } else {
            LOG_WARN("Failed to start IPv6 API server (continuing with IPv4 only)");
        }
    }

    // Check if at least one socket was created successfully
    if (sockfd_v4 < 0 && sockfd_v6 < 0) {
        LOG_ERROR("Failed to start API server on any protocol");
        return;
    }

    started = true;
}

WiFiClient LwipDualStackServer::accept()
{
    if (!started) {
        return WiFiClient();
    }

    // Try IPv4 socket first
    if (sockfd_v4 >= 0) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);

        int client_fd = ::accept(sockfd_v4, (struct sockaddr *)&source_addr, &addr_len);
        if (client_fd >= 0) {
            // Successfully accepted IPv4 connection
            char addr_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &source_addr.sin_addr, addr_str, sizeof(addr_str));
            LOG_INFO("Accepted IPv4 connection from %s:%d", addr_str, ntohs(source_addr.sin_port));

            // Wrap socket fd in WiFiClient
            return WiFiClient(client_fd);
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // Real error (not just "no connection pending")
            LOG_ERROR("IPv4 accept error: errno=%d", errno);
        }
    }

    // Try IPv6 socket if enabled
    if (sockfd_v6 >= 0) {
        struct sockaddr_in6 source_addr;
        socklen_t addr_len = sizeof(source_addr);

        int client_fd = ::accept(sockfd_v6, (struct sockaddr *)&source_addr, &addr_len);
        if (client_fd >= 0) {
            // Successfully accepted IPv6 connection
            char addr_str[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &source_addr.sin6_addr, addr_str, sizeof(addr_str));
            LOG_INFO("Accepted IPv6 connection from [%s]:%d", addr_str, ntohs(source_addr.sin6_port));

            // Wrap socket fd in WiFiClient
            return WiFiClient(client_fd);
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // Real error (not just "no connection pending")
            LOG_ERROR("IPv6 accept error: errno=%d", errno);
        }
    }

    // No connections pending on either socket
    return WiFiClient();
}

void LwipDualStackServer::end()
{
    if (sockfd_v4 >= 0) {
        close(sockfd_v4);
        sockfd_v4 = -1;
        LOG_DEBUG("Closed IPv4 API server socket");
    }

    if (sockfd_v6 >= 0) {
        close(sockfd_v6);
        sockfd_v6 = -1;
        LOG_DEBUG("Closed IPv6 API server socket");
    }

    started = false;
}

#endif // HAS_WIFI
