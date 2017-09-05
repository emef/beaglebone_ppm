/*
  author: Matt Forbes <matt.r.forbes@gmail.com>

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software
  and associated documentation files (the "Software"), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify, merge, publish, distribute,
  sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or
  substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
  BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include <stdint.h>
#include <stdio.h>
#include <pru_cfg.h>
#include <pru_intc.h>
#include <rsc_types.h>
#include <pru_ctrl.h>
#include <pru_rpmsg.h>
#include <string.h>
#include "resource_table.h"

volatile register uint32_t __R31;

/* Host-0 Interrupt sets bit 30 in register R31 */
#define HOST_INT			((uint32_t) 1 << 30)

/* The PRU-ICSS system events used for RPMsg are defined in the Linux device tree
 * PRU0 uses system event 16 (To ARM) and 17 (From ARM)
 * PRU1 uses system event 18 (To ARM) and 19 (From ARM)
 */
#define TO_ARM_HOST			16
#define FROM_ARM_HOST			17

/*
 * Using the name 'rpmsg-pru' will probe the rpmsg_pru driver found
 * at linux-x.y.z/drivers/rpmsg/rpmsg_pru.c
 */
#define CHAN_NAME			"rpmsg-pru"
#define CHAN_DESC			"Channel 30"
#define CHAN_PORT			30

/*
 * Used to make sure the Linux drivers are ready for RPMsg communication
 * Found at linux-x.y.z/include/uapi/linux/virtio_config.h
 */
#define VIRTIO_CONFIG_S_DRIVER_OK	4

// Pin we will read ppm signal from. TODO: this could be configurable
#define GPI_PIN 0x000F

#define SLOP_US                   20
#define CYCLES_PER_US             200
#define PULSE_WIDTH_US            300
#define MAX_CHANNEL_WIDTH_US      (2000 - PULSE_WIDTH_US) + SLOP_US
#define MAX_CHANNEL_WIDTH_CYCLES  (CYCLES_PER_US * MAX_CHANNEL_WIDTH_US)
#define MAX_PULSE_WIDTH_US        PULSE_WIDTH_US + SLOP_US
#define MAX_PULSE_WIDTH_CYCLES    (CYCLES_PER_US * MAX_PULSE_WIDTH_US)
#define CHANNELS_SIZE 12 * sizeof(uint16_t)

#define CONSTRAIN(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

uint8_t payload[RPMSG_BUF_SIZE];

void main(void)
{
    struct pru_rpmsg_transport transport;
    uint16_t src, dst, len;
    volatile uint8_t *status;

    CT_CFG.SYSCFG_bit.STANDBY_INIT = 0;
    CT_INTC.SICR_bit.STS_CLR_IDX = FROM_ARM_HOST;

    // turn on cycle counter
    PRU0_CTRL.CTRL_bit.CTR_EN = 1;

    status = &resourceTable.rpmsg_vdev.status;
    while (!(*status & VIRTIO_CONFIG_S_DRIVER_OK));

    pru_rpmsg_init(
        &transport,
        &resourceTable.rpmsg_vring0,
        &resourceTable.rpmsg_vring1,
        TO_ARM_HOST,
        FROM_ARM_HOST);

    while (PRU_RPMSG_SUCCESS != pru_rpmsg_channel(
               RPMSG_NS_CREATE,
               &transport,
               CHAN_NAME,
               CHAN_DESC,
               CHAN_PORT));

    uint8_t current_chan = 0;
    uint16_t channels[12] = { 1000 };
    uint32_t state, last_state = __R31 & GPI_PIN;
    uint32_t width;

    while (1) {

        // Check bit 30 of register R31 to see if the ARM has kicked us.
        if (__R31 & HOST_INT) {
            // Clear the event status.
            CT_INTC.SICR_bit.STS_CLR_IDX = FROM_ARM_HOST;

            // Receive all available messages, multiple messages can be sent per kick.
            while (PRU_RPMSG_SUCCESS == pru_rpmsg_receive(
                       &transport, &src, &dst, payload, &len));

            // Send the channels data to caller.
            pru_rpmsg_send(&transport, dst, src, channels, CHANNELS_SIZE);
        }

        if (current_chan >= 12) {
            // We've received all 12 channels, reset.
            current_chan = 0;
        }

        state = __R31 & GPI_PIN;

        if (state > last_state) {
            // rising edge.
            width = PRU0_CTRL.CYCLE;
            PRU0_CTRL.CYCLE = 0;

            if (width > MAX_CHANNEL_WIDTH_CYCLES) {
                // End of packet rest: reset.
                current_chan = 0;

            } else {
                channels[current_chan]
                    = CONSTRAIN(width / CYCLES_PER_US + PULSE_WIDTH_US, 1000, 2000);
                current_chan++;
            }

        } else if (state < last_state) {
            // falling edge.
            width = PRU0_CTRL.CYCLE;
            PRU0_CTRL.CYCLE = 0;

            if (width > MAX_PULSE_WIDTH_CYCLES) {
                // invalid, reset?
            }
        }

        last_state = state;
    }
}
