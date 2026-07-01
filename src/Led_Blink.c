/*
 * Led_Blink.c — LED Control Module
 *
 * Encapsulates all LED hardware interaction.
 * V1: Only LED0 blinks.
 * V2: Both LED0 and LED1 blink.
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "Led_Blink.h"

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

void Led_Blink_init(void)
{
	if (gpio_is_ready_dt(&led0)) {
		gpio_pin_configure_dt(&led0, GPIO_OUTPUT_ACTIVE);
	}
	if (gpio_is_ready_dt(&led1)) {
		gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
	}
}

void Led_Blink_toggle(void)
{
	if (gpio_is_ready_dt(&led0)) {
		gpio_pin_toggle_dt(&led0);
	}
	if (gpio_is_ready_dt(&led1)) {
		gpio_pin_toggle_dt(&led1);
	}
}
