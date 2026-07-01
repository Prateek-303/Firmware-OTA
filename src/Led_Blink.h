/*
 * Led_Blink.h — LED Control Public API
 *
 * Handles all LED initialization and toggling.
 * Completely independent from sensor and OTA logic.
 */

#ifndef Led_Blink_H_
#define Led_Blink_H_

/**
 * @brief Configure LED GPIOs.
 */
void Led_Blink_init(void);

/**
 * @brief Toggle all active LEDs once.
 */
void Led_Blink_toggle(void);

#endif /* Led_Blink_H_ */
