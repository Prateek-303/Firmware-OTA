/*
 * Sensors_OTA.c — Temperature Sensor Module
 *
 * Encapsulates all BMP280 + TMP117 I2C communication,
 * calibration, compensation math, and display logic.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <math.h>

#include "Sensors_OTA.h"

LOG_MODULE_REGISTER(Sensors_OTA, LOG_LEVEL_INF);

/* -------------------------------------------------------------------------
 * I2C Device Tree Nodes
 * -------------------------------------------------------------------------*/
#define BMP_NODE DT_NODELABEL(bmp280)
#define TMP_NODE DT_NODELABEL(tmp117)

static const struct i2c_dt_spec bmp = I2C_DT_SPEC_GET(BMP_NODE);
static const struct i2c_dt_spec tmp = I2C_DT_SPEC_GET(TMP_NODE);

/* Runtime fallback spec for BMP280 at 0x77 (SDO pulled HIGH) */
static struct i2c_dt_spec bmp_alt;
static const struct i2c_dt_spec *bmp_active = &bmp; /* Points to working address */

/* -------------------------------------------------------------------------
 * BMP280 Registers & Constants
 * -------------------------------------------------------------------------*/
#define BMP_CTRLMEAS           0xF4
#define BMP_CALIB00            0x88
#define BMP_ID                 0xD0
#define BMP_TEMPMSB            0xFA
#define BMP_CHIP_ID            0x58
#define BMP_CONFIG_VALUE       0x93

/* -------------------------------------------------------------------------
 * TMP117 Registers & Constants
 * -------------------------------------------------------------------------*/
#define TMP117_TEMP_REG        0x00
#define TMP117_DEVICE_ID_REG   0x0F
#define TMP117_DEVICE_ID       0x0117

/* -------------------------------------------------------------------------
 * BMP280 Calibration Data
 * -------------------------------------------------------------------------*/
static struct {
	uint16_t dig_t1;
	int16_t dig_t2;
	int16_t dig_t3;
} bmpdata;

static void bmp_calibrationdata(const struct i2c_dt_spec *spec)
{
	uint8_t values[6];
	if (i2c_burst_read_dt(spec, BMP_CALIB00, values, 6) != 0) {
		LOG_ERR("BMP280 calibration read failed");
		return;
	}
	bmpdata.dig_t1 = ((uint16_t)values[1] << 8) | values[0];
	bmpdata.dig_t2 = ((int16_t)values[3] << 8) | values[2];
	bmpdata.dig_t3 = ((int16_t)values[5] << 8) | values[4];
}

static int32_t bmp280_compensate_temp(int32_t adc_temp)
{
	int32_t var1, var2;
	var1 = (((adc_temp >> 3) - ((int32_t)bmpdata.dig_t1 << 1)) *
		((int32_t)bmpdata.dig_t2)) >> 11;
	var2 = ((((((adc_temp >> 4) - ((int32_t)bmpdata.dig_t1)) *
		((adc_temp >> 4) - ((int32_t)bmpdata.dig_t1))) >> 12) *
		((int32_t)bmpdata.dig_t3)) >> 14);
	return ((var1 + var2) * 5 + 128) >> 8;
}

/* -------------------------------------------------------------------------
 * Internal: Init & Read BMP280
 * -------------------------------------------------------------------------*/
static int bmp280_init(void)
{
	uint8_t id = 0;
	uint8_t reg = BMP_ID;
	int ret;

	/* --- Try primary address 0x76 (SDO pulled LOW / floating) --- */
	ret = i2c_write_read_dt(&bmp, &reg, 1, &id, 1);
	if (ret != 0) {
		LOG_WRN("BMP280 not found at 0x76 (err %d), trying 0x77...", ret);

		/* --- Fallback: try 0x77 (SDO pulled HIGH) --- */
		bmp_alt = bmp;
		bmp_alt.addr = 0x77;
		ret = i2c_write_read_dt(&bmp_alt, &reg, 1, &id, 1);
		if (ret != 0) {
			LOG_ERR("BMP280 not found at 0x77 either (err %d)", ret);
			LOG_ERR("Check wiring: SCL=P1.02 SDA=P1.03, 3.3V power, GND");
			return -1;
		}
		LOG_INF("BMP280 found at fallback address 0x77");
		bmp_active = &bmp_alt;
	} else {
		LOG_INF("BMP280 found at 0x76");
		bmp_active = &bmp;
	}

	if (id != BMP_CHIP_ID) {
		LOG_ERR("Invalid BMP280 chip ID: 0x%02X (expected 0x58)", id);
		return -1;
	}

	bmp_calibrationdata(bmp_active);
	uint8_t config[] = { BMP_CTRLMEAS, BMP_CONFIG_VALUE };
	if (i2c_write_dt(bmp_active, config, sizeof(config)) != 0) {
		LOG_ERR("BMP280 config write failed");
		return -1;
	}
	return 0;
}

static float read_bmp280_temp(void)
{
	uint8_t temp_val[3];
	if (i2c_burst_read_dt(bmp_active, BMP_TEMPMSB, temp_val, 3) != 0) {
		return -999.0f;
	}
	int32_t adc_temp = ((int32_t)temp_val[0] << 12) |
			   ((int32_t)temp_val[1] << 4) |
			   ((int32_t)temp_val[2] >> 4);
	return (float)bmp280_compensate_temp(adc_temp) / 100.0f;
}

/* -------------------------------------------------------------------------
 * Internal: Init & Read TMP117
 * -------------------------------------------------------------------------*/
static int tmp117_init(void)
{
	uint8_t reg = TMP117_DEVICE_ID_REG;
	uint8_t id_buf[2];
	if (i2c_write_read_dt(&tmp, &reg, 1, id_buf, 2) != 0) {
		LOG_ERR("TMP117 ID read failed");
		return -1;
	}
	uint16_t id = ((uint16_t)id_buf[0] << 8) | id_buf[1];
	if (id != TMP117_DEVICE_ID) {
		LOG_ERR("Invalid TMP117 ID: 0x%04X", id);
		return -1;
	}
	return 0;
}

static float read_tmp117_temp(void)
{
	uint8_t reg = TMP117_TEMP_REG;
	uint8_t data[2];
	if (i2c_write_read_dt(&tmp, &reg, 1, data, 2) != 0) {
		return -999.0f;
	}
	int16_t raw = ((int16_t)data[0] << 8) | data[1];
	return raw * 0.0078125f;
}

/* -------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------*/
int Sensors_OTA_init(void)
{
	if (!i2c_is_ready_dt(&bmp)) {
		LOG_ERR("I2C bus not ready — check CONFIG_I2C=y and overlay");
		return -1;
	}
	LOG_INF("I2C bus ready. Scanning for sensors...");

	if (bmp280_init() != 0) {
		LOG_ERR("BMP280 init failed — is the sensor wired to P1.02(SCL) P1.03(SDA)?");
		return -1;
	}
	if (tmp117_init() != 0) {
		LOG_ERR("TMP117 init failed — check address 0x48");
		return -1;
	}
	LOG_INF("BMP280 + TMP117 initialized successfully!");
	return 0;
}

void Sensors_OTA_read_and_log(void)
{
	float bmp_temp = read_bmp280_temp();
	float tmp_temp = read_tmp117_temp();

	if (bmp_temp == -999.0f || tmp_temp == -999.0f) {
		LOG_WRN("Failed to read sensors");
		return;
	}

	float arithmetic_mean = (bmp_temp + tmp_temp) / 2.0f;
	float weighted_mean = (0.3f * bmp_temp) + (0.7f * tmp_temp);
	float difference = fabsf(bmp_temp - tmp_temp);
	float error_percent = (difference / fabsf(tmp_temp)) * 100.0f;

	printk("\n====================================================\n");
	printk("BMP280 Temperature : %.2f C\n", (double)bmp_temp);
	printk("TMP117 Temperature : %.2f C\n\n", (double)tmp_temp);
	printk("Arithmetic Mean    : %.2f C\n", (double)arithmetic_mean);
	printk("Weighted Mean      : %.2f C\n\n", (double)weighted_mean);
	printk("Difference         : %.2f C\n", (double)difference);
	printk("Error Percentage   : %.2f %%\n", (double)error_percent);

	if (error_percent < 3.0f) {
		printk("Status             : NORMAL\n");
	} else if (error_percent < 5.0f) {
		printk("Status             : WARNING\n");
	} else {
		printk("Status             : SENSOR MISMATCH\n");
	}
	printk("Reference Sensor   : TMP117\n");
	printk("====================================================\n");
}
