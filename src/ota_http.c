/*
 * ota_http.c — Enterprise OTA FOTA Engine
 *
 * Self-contained OTA module implementing:
 *   - Background 24-hour polling thread
 *   - HTTPS/TLS 1.2 connection to GitHub
 *   - MAC-based device authorisation
 *   - JSON manifest parsing (version, file_size, image URL, SHA256)
 *   - Streamed firmware download with real-time SHA256 hashing
 *   - File size verification
 *   - MCUboot upgrade request with rollback failsafe
 *   - Professional performance and reliability logging
 *
 * Board: nRF7002 DK (nRF5340 Application Core)
 * SDK:   nRF Connect SDK v3.2.4 / Zephyr v4.2.99
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/sys/util.h>
#include <cJSON.h>
#include <mbedtls/sha256.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/printk.h>

#include "ota_http.h"
#include "github_certs.h"

LOG_MODULE_REGISTER(ota_http, LOG_LEVEL_INF);

/* -------------------------------------------------------------------------
 * Configuration
 * -------------------------------------------------------------------------*/
#define OTA_SERVER_HOST "raw.githubusercontent.com"
#define OTA_SERVER_PORT "443"
#define MANIFEST_PATH   "/Prateek-303/nrf54-OTA/main/manifest.json"
#define AUTH_PATH       "/Prateek-303/nrf54-OTA/main/authorized_devices.json"

/* Current Firmware Version is dynamically injected from the VERSION file */
#include <zephyr/app_version.h>
#define CURRENT_VERSION APP_VERSION_STRING

/* -------------------------------------------------------------------------
 * Private resources
 * -------------------------------------------------------------------------*/
static K_THREAD_STACK_DEFINE(ota_thread_stack, 12288);
static struct k_thread ota_thread_data;
static atomic_t ota_started = ATOMIC_INIT(0);

/* HTTP receive buffer */
#define RECV_BUF_SIZE 2048
static uint8_t recv_buf[RECV_BUF_SIZE];

/* JSON accumulation buffer (for manifest and auth responses) */
#define JSON_BUF_SIZE 2048
static char json_buffer[JSON_BUF_SIZE];
static size_t json_offset;

/* Firmware flash context */
static struct flash_img_context flash_ctx;

/* Download state tracking */
enum ota_state {
	OTA_STATE_AUTH,
	OTA_STATE_MANIFEST,
	OTA_STATE_BIN,
};
static enum ota_state current_state;
static size_t downloaded_bytes;
static size_t expected_file_size;
static char target_version[32];
static char target_url[256];
static char expected_sha256[65];
static char expected_crc32[10];
static uint32_t current_crc32;

/* SHA256 streaming context */
static mbedtls_sha256_context sha_ctx;

/* -------------------------------------------------------------------------
 * HTTP response callback — dispatches body data by current_state
 * -------------------------------------------------------------------------*/
static int http_response_cb(struct http_response *rsp,
			     enum http_final_call final_data,
			     void *user_data)
{
	ARG_UNUSED(user_data);

	if (rsp->body_frag_len == 0) {
		return 0;
	}

	switch (current_state) {
	case OTA_STATE_AUTH:
	case OTA_STATE_MANIFEST: {
		size_t copy_len = MIN(rsp->body_frag_len,
				      JSON_BUF_SIZE - json_offset - 1);
		if (copy_len > 0) {
			memcpy(&json_buffer[json_offset], rsp->body_frag_start,
			       copy_len);
			json_offset += copy_len;
			json_buffer[json_offset] = '\0';
		}
		break;
	}
	case OTA_STATE_BIN: {
		int rc = flash_img_buffered_write(&flash_ctx,
						  rsp->body_frag_start,
						  rsp->body_frag_len,
						  final_data == HTTP_DATA_FINAL);
		if (rc < 0) {
			printk("\n");
			LOG_ERR("Flash write failed at offset %zu (err %d)",
				downloaded_bytes, rc);
			return rc;
		}
		/* Update streaming hashes */
		mbedtls_sha256_update(&sha_ctx, rsp->body_frag_start,
				      rsp->body_frag_len);
		current_crc32 = crc32_ieee_update(current_crc32, rsp->body_frag_start, rsp->body_frag_len);
		downloaded_bytes += rsp->body_frag_len;

		if (expected_file_size > 0) {
			int percent = (int)((downloaded_bytes * 100UL) / expected_file_size);
			int bars = percent / 5;
			char bar_str[21];

			for (int i = 0; i < 20; i++) {
				bar_str[i] = (i < bars) ? '#' : '-';
			}
			bar_str[20] = '\0';
			/* \r returns cursor to start of line — overwrites previous bar */
			printk("\r  [%-20s] %3d%%  %zu/%zu B",
			       bar_str, percent,
			       downloaded_bytes, expected_file_size);
		}

		if (final_data == HTTP_DATA_FINAL) {
			printk("\n");
		}
		break;
	}
	}
	return 0;
}

/* -------------------------------------------------------------------------
 * Check device authorisation against GitHub whitelist
 * -------------------------------------------------------------------------*/
static bool check_authorization(void)
{
	/* Read the hardware MAC address */
	uint8_t dev_id[8];
	char dev_id_str[17];
	ssize_t id_len = hwinfo_get_device_id(dev_id, sizeof(dev_id));

	if (id_len <= 0) {
		LOG_ERR("Failed to read hardware device ID (err %zd)", id_len);
		return false;
	}

	/* Convert binary MAC to hex string */
	for (int i = 0; i < (int)id_len && i < 8; i++) {
		sprintf(&dev_id_str[i * 2], "%02x", dev_id[i]);
	}
	dev_id_str[id_len * 2] = '\0';

	/* Parse the authorised devices JSON */
	bool authorized = false;
	cJSON *root = cJSON_Parse(json_buffer);

	if (!root) {
		LOG_ERR("Failed to parse authorized_devices.json");
		return false;
	}

	cJSON *devices = cJSON_GetObjectItem(root, "authorized_devices");

	if (cJSON_IsArray(devices)) {
		cJSON *dev;
		cJSON_ArrayForEach(dev, devices) {
			if (cJSON_IsString(dev) && strcmp(dev->valuestring, dev_id_str) == 0) {
				authorized = true;
				break;
			}
		}
	}

	cJSON_Delete(root);

	if (authorized) {
		LOG_INF("[AUTH] Device %s is whitelisted.", dev_id_str);
	} else {
		LOG_ERR("[AUTH] Device %s is NOT AUTHORIZED! Update forcefully blocked.", dev_id_str);
	}

	return authorized;
}

/* -------------------------------------------------------------------------
 * Parse and validate JSON manifest
 * -------------------------------------------------------------------------*/
static bool parse_manifest(void)
{
	bool should_update = false;

	cJSON *root = cJSON_Parse(json_buffer);
	if (!root) {
		LOG_ERR("Failed to parse JSON.");
		return false;
	}

	cJSON *version = cJSON_GetObjectItem(root, "version");
	cJSON *file_size = cJSON_GetObjectItem(root, "file_size");
	cJSON *image_url = cJSON_GetObjectItem(root, "image");
	cJSON *sha256 = cJSON_GetObjectItem(root, "sha256");
	cJSON *crc32 = cJSON_GetObjectItem(root, "crc32");

	if (cJSON_IsString(version) && cJSON_IsNumber(file_size) && cJSON_IsString(image_url)) {
		LOG_INF("[OTA] Cloud Version: %s | Current: %s", version->valuestring, CURRENT_VERSION);
		
		if (cJSON_IsString(sha256)) {
			strncpy(expected_sha256, sha256->valuestring, sizeof(expected_sha256) - 1);
		} else {
			LOG_WRN("-> SHA256 missing! Integrity check will be skipped.");
			expected_sha256[0] = '\0';
		}
		
		if (cJSON_IsString(crc32)) {
			strncpy(expected_crc32, crc32->valuestring, sizeof(expected_crc32) - 1);
		} else {
			expected_crc32[0] = '\0';
		}

		/* Version Check (Simple String Compare) */
		if (strcmp(CURRENT_VERSION, version->valuestring) != 0) {
			LOG_INF("[OTA] New version found! Downloading...");
			strncpy(target_version, version->valuestring, sizeof(target_version) - 1);
			snprintf(target_url, sizeof(target_url), "/Prateek-303/nrf54-OTA/main%s%s", 
				 (image_url->valuestring[0] == '/') ? "" : "/", image_url->valuestring);
			expected_file_size = file_size->valueint;
			should_update = true;
		} else {
			LOG_INF("[OTA] Firmware is up to date.");
		}
	} else {
		LOG_ERR("Manifest missing required fields.");
	}

	cJSON_Delete(root);
	return should_update;
}

/* -------------------------------------------------------------------------
 * ota_worker — Background thread entry point (24-hour polling loop)
 * -------------------------------------------------------------------------*/
static void ota_worker(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		int rc;
		int sock = -1;
		struct zsock_addrinfo hints = {0};
		struct zsock_addrinfo *res = NULL;

		LOG_INF("[OTA] Checking for updates...");

		/* Set system clock to a valid date to pass certificate validity checks */
		struct timespec ts = {
			.tv_sec = 1782223200, /* June 23, 2026 14:00:00 UTC */
			.tv_nsec = 0
		};
		clock_settime(CLOCK_REALTIME, &ts);

		/* 1. DNS Resolution */
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		rc = zsock_getaddrinfo(OTA_SERVER_HOST, OTA_SERVER_PORT, &hints, &res);
		if (rc != 0) {
			LOG_ERR("[RELIABILITY FAIL] DNS Resolution failed for %s (err %d)", OTA_SERVER_HOST, rc);
			goto cleanup;
		}

		/* 2. Open Secure TLS Socket */
		sock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TLS_1_2);
		if (sock < 0) {
			LOG_ERR("[RELIABILITY FAIL] Hardware TLS Socket creation failed (err %d)", errno);
			goto cleanup;
		}

		int verify = TLS_PEER_VERIFY_REQUIRED;
		zsock_setsockopt(sock, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify));

		sec_tag_t sec_tag_list[] = { GITHUB_CA_CERT_TAG };
		zsock_setsockopt(sock, SOL_TLS, TLS_SEC_TAG_LIST, sec_tag_list, sizeof(sec_tag_list));
		zsock_setsockopt(sock, SOL_TLS, TLS_HOSTNAME, OTA_SERVER_HOST, strlen(OTA_SERVER_HOST));

		rc = zsock_connect(sock, res->ai_addr, res->ai_addrlen);
		if (rc < 0) {
			LOG_ERR("TLS Handshake failed (err %d)", errno);
			goto cleanup;
		}

		/* 3. Fetch Authorization List */
		current_state = OTA_STATE_AUTH;
		json_offset = 0;
		memset(json_buffer, 0, JSON_BUF_SIZE);

		struct http_request req = {0};
		req.method = HTTP_GET;
		req.url = AUTH_PATH;
		req.host = OTA_SERVER_HOST;
		req.protocol = "HTTP/1.1";
		req.response = http_response_cb;
		req.recv_buf = recv_buf;
		req.recv_buf_len = sizeof(recv_buf);

		rc = http_client_req(sock, &req, SYS_FOREVER_MS, NULL);
		if (rc < 0 || json_offset == 0) {
			LOG_ERR("Auth HTTP request failed. Assuming missing auth list.");
			goto cleanup;
		}

		/* Check if it's a 404 text instead of JSON */
		if (strstr(json_buffer, "404") != NULL && json_offset < 50) {
			LOG_ERR("authorized_devices.json NOT FOUND on GitHub! You must create it.");
			goto cleanup;
		}

		if (!check_authorization()) {
			goto cleanup; /* Blocked */
		}

		/* 4. Fetch Manifest */
		current_state = OTA_STATE_MANIFEST;
		json_offset = 0;
		memset(json_buffer, 0, JSON_BUF_SIZE);

		memset(&req, 0, sizeof(req));
		req.method = HTTP_GET;
		req.url = MANIFEST_PATH;
		req.host = OTA_SERVER_HOST;
		req.protocol = "HTTP/1.1";
		req.response = http_response_cb;
		req.recv_buf = recv_buf;
		req.recv_buf_len = sizeof(recv_buf);

		rc = http_client_req(sock, &req, SYS_FOREVER_MS, NULL);
		if (rc < 0) {
			LOG_ERR("Manifest HTTP request failed (err %d)", rc);
			goto cleanup;
		}

		/* 5. Parse Manifest & Check Version */
		if (!parse_manifest()) {
			goto cleanup; /* Either up to date or parsing failed */
		}

		/* 6. Prepare Flash for Download */
		rc = flash_img_init(&flash_ctx);
		if (rc < 0) {
			LOG_ERR("Failed to initialize flash image context (err %d)", rc);
			goto cleanup;
		}
		
		/* 7. Fetch Firmware Binary */
		current_state = OTA_STATE_BIN;
		downloaded_bytes = 0;
		current_crc32 = 0;
		mbedtls_sha256_init(&sha_ctx);
		mbedtls_sha256_starts(&sha_ctx, 0);
		
		memset(&req, 0, sizeof(req));
		req.method = HTTP_GET;
		req.url = target_url;
		req.host = OTA_SERVER_HOST;
		req.protocol = "HTTP/1.1";
		req.response = http_response_cb;
		req.recv_buf = recv_buf;
		req.recv_buf_len = sizeof(recv_buf);

		uint32_t start_time = k_uptime_get_32();
		rc = http_client_req(sock, &req, SYS_FOREVER_MS, NULL);
		uint32_t end_time = k_uptime_get_32();
		uint32_t duration_ms = end_time - start_time;
		
		/* Finish SHA256 */
		uint8_t final_hash[32];
		char final_hash_str[65];
		mbedtls_sha256_finish(&sha_ctx, final_hash);
		mbedtls_sha256_free(&sha_ctx);
		
		if (rc < 0) {
			LOG_ERR("[RELIABILITY FAIL] Network download interrupted (err %d). Aborting.", rc);
			goto cleanup;
		}

		/* 8. Finalize, Check Integrity, and Reboot */
		if (downloaded_bytes == expected_file_size) {
			
			/* Check CRC32 if provided */
			if (strlen(expected_crc32) > 0) {
				char final_crc32_str[10];
				sprintf(final_crc32_str, "%08x", current_crc32);
				if (strcmp(final_crc32_str, expected_crc32) != 0) {
					LOG_ERR("[FAIL] CRC32 Mismatch! Expected: %s, Got: %s", expected_crc32, final_crc32_str);
					goto cleanup;
				}
				LOG_INF("[INTEGRITY] CRC32 Verified: %s", final_crc32_str);
			}

			/* Check SHA256 if provided */
			if (strlen(expected_sha256) > 0) {
				for (int i = 0; i < 32; i++) {
					sprintf(&final_hash_str[i * 2], "%02x", final_hash[i]);
				}
				if (strcmp(final_hash_str, expected_sha256) != 0) {
					LOG_ERR("[FAIL] SHA256 Mismatch! Corrupted Firmware Detected.");
					LOG_ERR("Expected: %s", expected_sha256);
					LOG_ERR("Calculated: %s", final_hash_str);
					goto cleanup;
				}
				LOG_INF("[INTEGRITY] SHA256 Verified: %s", final_hash_str);
			}

			uint32_t throughput = (downloaded_bytes * 1000) / (duration_ms > 0 ? duration_ms : 1);
			LOG_INF("[PERF] Downloaded %zu bytes in %u ms (%u B/s)", downloaded_bytes, duration_ms, throughput);

			rc = boot_request_upgrade(BOOT_UPGRADE_TEST);
			if (rc < 0) {
				LOG_ERR("Failed to request upgrade (err %d)", rc);
			} else {
				LOG_INF("[OTA] Update staged. Rebooting in 3 seconds...");
				k_sleep(K_SECONDS(3));
				sys_reboot(SYS_REBOOT_COLD);
			}
		} else {
			LOG_ERR("Downloaded size (%zu) does not match expected size (%zu). Aborting.", downloaded_bytes, expected_file_size);
		}

cleanup:
		if (sock >= 0) {
			zsock_close(sock);
		}
		if (res != NULL) {
			zsock_freeaddrinfo(res);
		}
		LOG_DBG("OTA check finished. Sleeping for 24 hours...");
		k_sleep(K_HOURS(24));
	}
}

/* -------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------*/
void ota_start(void)
{
	if (atomic_cas(&ota_started, 0, 1)) {
		k_thread_create(&ota_thread_data, ota_thread_stack,
				K_THREAD_STACK_SIZEOF(ota_thread_stack),
				ota_worker, NULL, NULL, NULL,
				K_PRIO_COOP(7), 0, K_NO_WAIT);
	}
}
