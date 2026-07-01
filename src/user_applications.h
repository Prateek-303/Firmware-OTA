/*
 * user_applications.h — Application Orchestrator Public API
 *
 * This module wires together the independent application layers:
 *   - Led_Blink    → LED control
 *   - Sensors_OTA → Temperature sensor monitoring
 *
 * The OTA pipeline (ota_http.c, wifi_mgr.c) calls ONLY these
 * two functions. To upgrade the product behaviour via OTA,
 * modify user_applications.c to uncomment/comment the module calls.
 */

#ifndef user_applications_H_
#define user_applications_H_

/**
 * @brief Initialise all application modules (LEDs, sensors, etc.).
 */
void user_applications_init(void);

/**
 * @brief Run the main application loop (never returns).
 */
void user_applications_run(void);

#endif /* user_applications_H_ */
