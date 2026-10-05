/*
 * FlySky FS-DB01 LED control module driver
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FSDB01_H_
#define FSDB01_H_

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Frame bits, sent bit 0 first (see README, FS-DB01 protocol) */
#define FSDB01_TURN_RIGHT     BIT(0)
#define FSDB01_TURN_LEFT      BIT(1)
#define FSDB01_ILLUMINATION_1 BIT(2)
#define FSDB01_ILLUMINATION_2 BIT(3)
#define FSDB01_REVERSE_BRAKE  BIT(5)
#define FSDB01_NO_SIGNAL_FAST BIT(6)
#define FSDB01_NO_SIGNAL_SLOW BIT(7)

#define FSDB01_FRAME_BITS 9
#define FSDB01_FRAME_MASK BIT_MASK(FSDB01_FRAME_BITS)

/* Power-on state, as in the reference firmware: no-signal indication */
#define FSDB01_FRAME_NO_SIGNAL (FSDB01_NO_SIGNAL_FAST | FSDB01_NO_SIGNAL_SLOW)

/**
 * @brief Set the frame that is sent repeatedly to the module.
 *
 * @param dev FS-DB01 device.
 * @param frame Frame bits (FSDB01_* flags).
 *
 * @retval 0 on success.
 * @retval -EINVAL if @p frame has bits outside the 9-bit frame.
 */
int fsdb01_set_frame(const struct device* dev, uint16_t frame);

/**
 * @brief Get the frame currently set for sending.
 *
 * @param dev FS-DB01 device.
 *
 * @return Frame bits (FSDB01_* flags).
 */
uint16_t fsdb01_get_frame(const struct device* dev);

/**
 * @brief Check whether frames are being sent.
 *
 * @param dev FS-DB01 device.
 *
 * @return true if enabled, false if the line is held low.
 */
bool fsdb01_is_enabled(const struct device* dev);

/**
 * @brief Start sending frames to the module.
 *
 * The first frame starts after a 3 ms low gap. Does nothing if already
 * enabled. The driver is enabled at boot.
 *
 * Must not be called from an ISR.
 *
 * @param dev FS-DB01 device.
 *
 * @retval 0 on success.
 * @retval <0 GPIO error.
 */
int fsdb01_enable(const struct device* dev);

/**
 * @brief Stop sending and hold the line low.
 *
 * Low is the protocol level, so the line is high if the devicetree property
 * @c invert is set. A frame in progress is cut off.
 *
 * Must not be called from an ISR.
 *
 * @param dev FS-DB01 device.
 *
 * @retval 0 on success.
 * @retval <0 GPIO error.
 */
int fsdb01_disable(const struct device* dev);

#ifdef __cplusplus
}
#endif

#endif /* FSDB01_H_ */
