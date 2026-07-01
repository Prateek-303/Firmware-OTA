/*
 * Sensors_OTA.h — Temperature Sensor Public API
 *
 * Handles BMP280 + TMP117 initialization and reading over I2C.
 * Completely independent from LED and OTA logic.
 */

#ifndef Sensors_OTA_H_
#define Sensors_OTA_H_

/**
 * @brief Initialize BMP280 and TMP117 sensors over I2C.
 * @return 0 on success, negative on failure.
 */
int Sensors_OTA_init(void);

/**
 * @brief Read both sensors and print the comparison table.
 */
void Sensors_OTA_read_and_log(void);

#endif /* Sensors_OTA_H_ */
