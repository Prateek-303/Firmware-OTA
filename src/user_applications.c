/*
 * user_applications.c — Application Orchestrator
 *
 * This file wires together the independent application modules:
 *   - Led_Blink.c    → LED blink patterns
 *   - Sensors_OTA.c → BMP280 + TMP117 temperature monitoring
 *
 * OTA Version Strategy:
 *   V1 (1.x.x): LED blink only    → sensor calls COMMENTED OUT
 *   V2 (2.x.x): LEDs + Sensors    → sensor calls UNCOMMENTED
 *
 * The OTA engine (ota_http.c, wifi_mgr.c) never touches this file.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "user_applications.h"
#include "Led_Blink.h"
#include "Sensors_OTA.h"    /* V1: Commented out — uncomment for V2 */

LOG_MODULE_REGISTER(user_applications, LOG_LEVEL_INF);

void user_applications_init(void)
{
	/* LED module — always active */
	Led_Blink_init();

	/* Sensor module — uncomment for V2 OTA update */
	 Sensors_OTA_init(); 
}

void user_applications_run(void)
{
	while (1) {
		/* LEDs — always toggle */
		Led_Blink_toggle();

		/* Sensors — uncomment for V2 OTA update */
		Sensors_OTA_read_and_log(); 

		k_sleep(K_MSEC(500));
	}
}
