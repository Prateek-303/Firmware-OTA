/*
 * ota_http.h — OTA Engine Public API
 *
 * This header exposes only the single entry point needed by the
 * application layer. All internal state, sockets, and crypto
 * contexts are private to ota_http.c.
 */

#ifndef OTA_HTTP_H_
#define OTA_HTTP_H_

/**
 * @brief Start the OTA background polling thread.
 *
 * Spawns a dedicated thread that periodically connects to the
 * configured GitHub repository over HTTPS, checks for new
 * firmware versions, validates device authorisation and binary
 * integrity, and triggers an MCUboot swap when appropriate.
 *
 * Safe to call exactly once after the network (Wi-Fi + DHCP)
 * is fully operational.
 */
void ota_start(void);

#endif /* OTA_HTTP_H_ */
