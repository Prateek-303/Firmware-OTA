/*
 * main.c — System Orchestrator
 *
 * This file contains ONLY system-level initialisation and lifecycle
 * management. All domain-specific logic is delegated to dedicated
 * modules:
 *
 *   wifi_mgr.c   — Wi-Fi connection lifecycle
 *   ota_http.c   — OTA polling, download, and integrity engine
 *   user_applications.c  — Product-specific behaviour (LED blink, etc.)
 *
 * To port the OTA system to a different product, replace ONLY
 * user_applications.c and user_applications.h. Everything else stays untouched.
 *
 * Board: nRF7002 DK (nRF5340 Application Core)
 * SDK:   nRF Connect SDK v3.2.4 / Zephyr v4.2.99
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/dfu/mcuboot.h>

#include "github_certs.h"
#include "wifi_mgr.h"
#include "ota_http.h"
#include "user_applications.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* -------------------------------------------------------------------------
 * Callback: Invoked by wifi_mgr when DHCP assigns an IP address
 * -------------------------------------------------------------------------*/
static void on_network_ready(const char *ip_str)
{
	LOG_INF("[NET] Ready — IP: %s", ip_str);
	ota_start();
}

/* -------------------------------------------------------------------------
 * main — System entry point
 * -------------------------------------------------------------------------*/
int main(void)
{
	LOG_INF("=== OTA Firmware System — Modular Architecture ===");

	/* 1. MCUboot image confirmation (rollback failsafe) */
#ifdef CONFIG_BOOTLOADER_MCUBOOT
	int rc = boot_write_img_confirmed();
	if (rc == 0) {
		LOG_INF("Firmware image confirmed successfully!");
	} else if (rc == -EALREADY) {
		LOG_INF("Firmware image was already confirmed.");
	} else {
		LOG_WRN("Failed to confirm firmware image: %d", rc);
	}
#endif

	/* 2. Register the GitHub Root CA for TLS Handshakes */
	int err = tls_credential_add(GITHUB_CA_CERT_TAG,
				     TLS_CREDENTIAL_CA_CERTIFICATE,
				     github_ca_cert,
				     sizeof(github_ca_cert));
	if (err < 0) {
		LOG_ERR("Failed to register CA cert: %d", err);
	}

	/* 3. Initialise Wi-Fi — OTA starts automatically on IP assignment */
	wifi_mgr_init(on_network_ready);

	/* 4. Initialise and run the product application */
	user_applications_init();
	user_applications_run(); /* Never returns */

	return 0;
}