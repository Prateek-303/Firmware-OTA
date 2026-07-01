/*
 * wifi_mgr.c — Wi-Fi Connection Manager
 *
 * Encapsulates all Wi-Fi lifecycle management:
 *   - nRF7002 firmware ready gate
 *   - Wi-Fi connect via stored credentials
 *   - DHCP bound event detection
 *   - User callback dispatch on successful IP assignment
 *
 * Board: nRF7002 DK (nRF5340 Application Core)
 * SDK:   nRF Connect SDK v3.2.4 / Zephyr v4.2.99
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>

#ifdef CONFIG_WIFI_READY_LIB
#include <net/wifi_ready.h>
#endif

#include "wifi_mgr.h"

LOG_MODULE_REGISTER(wifi_mgr, LOG_LEVEL_INF);

/* -------------------------------------------------------------------------
 * Private state
 * -------------------------------------------------------------------------*/
static struct net_mgmt_event_callback wifi_cb;
static struct net_mgmt_event_callback dhcp_cb;
static wifi_connected_cb_t user_connected_cb;

#ifdef CONFIG_WIFI_READY_LIB
static K_SEM_DEFINE(wifi_ready_sem, 0, 1);
static bool wifi_ready_status;
#endif

/* -------------------------------------------------------------------------
 * Internal event handlers
 * -------------------------------------------------------------------------*/
static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				     uint64_t mgmt_event, struct net_if *iface)
{
	ARG_UNUSED(cb);
	ARG_UNUSED(iface);

	if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
		const struct wifi_status *status = cb->info;

		if (status->status) {
			LOG_ERR("Wi-Fi connection failed (%d)", status->status);
		} else {
			LOG_INF("Wi-Fi connected — waiting for DHCP...");
		}
	} else if (mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
		LOG_WRN("Wi-Fi disconnected");
	}
}

static void dhcp_event_handler(struct net_mgmt_event_callback *cb,
			       uint64_t mgmt_event, struct net_if *iface)
{
	ARG_UNUSED(iface);

	if (mgmt_event == NET_EVENT_IPV4_DHCP_BOUND) {
		const struct net_if_dhcpv4 *dhcpv4 = cb->info;
		char ip_str[NET_IPV4_ADDR_LEN];

		net_addr_ntop(AF_INET, &dhcpv4->requested_ip,
			      ip_str, sizeof(ip_str));

		if (user_connected_cb) {
			user_connected_cb(ip_str);
		}
	}
}

/* -------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------*/
static int wifi_connect(void)
{
	struct net_if *iface = net_if_get_first_wifi();

	if (!iface) {
		LOG_ERR("No Wi-Fi interface found");
		return -ENODEV;
	}

	int rc = net_mgmt(NET_REQUEST_WIFI_CONNECT_STORED, iface, NULL, 0);

	if (rc) {
		LOG_ERR("Connect request failed: %d", rc);
	}
	return rc;
}

#ifdef CONFIG_WIFI_READY_LIB
static void wifi_ready_cb(bool ready)
{
	wifi_ready_status = ready;
	k_sem_give(&wifi_ready_sem);
}
#endif

/* -------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------*/
void wifi_mgr_init(wifi_connected_cb_t cb)
{
	user_connected_cb = cb;

	/* Register Wi-Fi and DHCP event callbacks */
	net_mgmt_init_event_callback(&wifi_cb, wifi_mgmt_event_handler,
				     NET_EVENT_WIFI_CONNECT_RESULT |
				     NET_EVENT_WIFI_DISCONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_cb);

	net_mgmt_init_event_callback(&dhcp_cb, dhcp_event_handler,
				     NET_EVENT_IPV4_DHCP_BOUND);
	net_mgmt_add_event_callback(&dhcp_cb);

#ifdef CONFIG_WIFI_READY_LIB
	wifi_ready_callback_t ready_cb = { .wifi_ready_cb = wifi_ready_cb };
	struct net_if *iface = net_if_get_first_wifi();

	if (iface && register_wifi_ready_callback(ready_cb, iface) == 0) {
		k_sem_take(&wifi_ready_sem, K_FOREVER);
		if (wifi_ready_status) {
			wifi_connect();
		} else {
			LOG_ERR("nRF7002 firmware failed to initialize");
		}
	} else {
		LOG_ERR("Could not register wifi_ready callback");
	}
#else
	wifi_connect();
#endif
}
