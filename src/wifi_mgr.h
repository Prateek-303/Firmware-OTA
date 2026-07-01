/*
 * wifi_mgr.h — Wi-Fi Connection Manager Public API
 *
 * Encapsulates the entire Wi-Fi lifecycle: event callbacks,
 * nRF7002 ready gate, credential-based connection, and DHCP.
 * The application layer registers a single callback that fires
 * when an IP address has been assigned.
 */

#ifndef WIFI_MGR_H_
#define WIFI_MGR_H_

/**
 * @brief Callback type invoked when DHCP assigns an IP address.
 *
 * @param ip_str  Null-terminated IPv4 address string (e.g. "192.168.1.100").
 */
typedef void (*wifi_connected_cb_t)(const char *ip_str);

/**
 * @brief Initialise the Wi-Fi subsystem and begin connection.
 *
 * Registers all required Zephyr net_mgmt callbacks internally,
 * waits for the nRF7002 firmware to become ready, issues a
 * connection request using stored credentials, and invokes
 * @p cb once DHCP binds an IPv4 address.
 *
 * @param cb  Function to call when the network is fully up.
 */
void wifi_mgr_init(wifi_connected_cb_t cb);

#endif /* WIFI_MGR_H_ */
