/*
 * Control thread: CRSF channels to FS-DB01 frames, with failsafe
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CONTROL_H_
#define CONTROL_H_

#include <zephyr/device.h>

#include <cstdint>

#include "fsdb01.h"

namespace control {

/* Sent until RC frames arrive, and after the link is lost */
constexpr uint16_t failsafe_frame = FSDB01_NO_SIGNAL_FAST;

/* Link counts as lost when no RC channels frame has arrived for this long */
constexpr int64_t link_timeout_ms = 500;

/* Start the control thread; enables FS-DB01 output with the failsafe frame */
int start(const device* fsdb01);

bool link_up();

/*
 * In manual mode the control thread stops setting frames, so the leds shell
 * commands can set them by hand.
 */
void set_manual(bool manual);
bool is_manual();

}  // namespace control

#endif /* CONTROL_H_ */
