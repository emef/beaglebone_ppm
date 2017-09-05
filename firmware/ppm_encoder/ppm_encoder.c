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
#include <pru_ctrl.h>
#include <pru_intc.h>
#include <rsc_types.h>
#include <pru_virtqueue.h>
#include <pru_rpmsg.h>

#include "resource_table.h"

volatile register uint32_t __R30;
volatile register uint32_t __R31;

/*
  Used to make sure the Linux drivers are ready for RPMsg communication
  Found at linux-x.y.z/include/uapi/linux/virtio_config.h
*/
#define VIRTIO_CONFIG_S_DRIVER_OK	4

// Pin mask to set/clear our gpo pin.
// TODO: this could be configurable
#define GPO_PIN 0x000F

#define CYCLES_PER_US        200
#define PULSE_WIDTH_US       300
#define PULSE_WIDTH_CYCLES   (PULSE_WIDTH_US * CYCLES_PER_US)
#define FRAME_LENGTH_US      22500
#define FRAME_LENGTH_CYCLES  (FRAME_LENGTH_US * CYCLES_PER_US)

#define CONSTRAIN(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

void main(void) {
    struct pru_rpmsg_transport transport;
    uint16_t src, dst;
    volatile uint8_t* status;

    // allow OCP master port access by the PRU so the PRU can read external memories
    CT_CFG.SYSCFG_bit.STANDBY_INIT = 0;

    // Turn on cycle counter
    PRU1_CTRL.CTRL_bit.CTR_EN = 1;

    // clear the status of the PRU-ICSS system event that the ARM will use to 'kick' us
    CT_INTC.SICR_bit.STS_CLR_IDX = FROM_ARM_HOST;

    // Make sure the Linux drivers are ready for RPMsg communication
    status = &resourceTable.rpmsg_vdev.status;
    while (!(*status & VIRTIO_CONFIG_S_DRIVER_OK));

    // Initialize pru_virtqueue corresponding to vring0 (PRU to ARM Host direction)
    pru_virtqueue_init(
        &transport.virtqueue0,
        &resourceTable.rpmsg_vring0,
        TO_ARM_HOST,
        FROM_ARM_HOST);

    // Initialize pru_virtqueue corresponding to vring1 (ARM Host to PRU direction)
    pru_virtqueue_init(
        &transport.virtqueue1,
        &resourceTable.rpmsg_vring1,
        TO_ARM_HOST,
        FROM_ARM_HOST);

    // Create the RPMsg channel between the PRU and ARM user space
    // using the transport structure.
    while (PRU_RPMSG_SUCCESS != pru_rpmsg_channel(
               RPMSG_NS_CREATE,
               &transport,
               CHAN_NAME,
               CHAN_DESC,
               CHAN_PORT));

    uint16_t channels[12] = { 1500 };
    uint16_t len;
    uint32_t cycle_target;
    uint8_t current_chan = 0;
    uint32_t current_frame_cycles = 0;

    // Initialize with output pin low.
    __R30 &= ~GPO_PIN;

    while (1) {
        // Check bit 30 of register R31 to see if the ARM has kicked us.
        // If so, read message(s) into `channels` to be written out as ppm.
        if (__R31 & HOST_INT) {
            // Clear the event status
            CT_INTC.SICR_bit.STS_CLR_IDX = FROM_ARM_HOST;

            // Receive all available messages, multiple messages can be sent per kick
            // Store message in `channels` to be written out in the next ppm packet.
            while (pru_rpmsg_receive(
                       &transport,
                       &src,
                       &dst,
                       channels,
                       &len) == PRU_RPMSG_SUCCESS);
        }

        if (PRU1_CTRL.CYCLE >= cycle_target) {
            // TIMER "INTERRUPT"

            // Reset cycle counter and toggle output pin.
            current_frame_cycles += PRU1_CTRL.CYCLE;
            PRU1_CTRL.CYCLE = 0;
            __R30 ^= GPO_PIN;

            if (__R30 & GPO_PIN) {
                // PIN HIGH

                // Pulse high for PULSE_WIDTH_CYCLES
                cycle_target = PULSE_WIDTH_CYCLES - PRU1_CTRL.CYCLE;

            } else {
                // PIN LOW

                if (current_chan == 0) {
                    current_frame_cycles = PULSE_WIDTH_CYCLES;
                }

                if (current_chan >= 12) {
                    // End of channels, rest until end of packet length.
                    current_chan = 0;
                    cycle_target
                        = FRAME_LENGTH_CYCLES - current_frame_cycles - PRU1_CTRL.CYCLE;
                } else {
                    // Rest width is a function of channel value
                    uint16_t rest_us
                        = CONSTRAIN(channels[current_chan], 1000, 2000) - PULSE_WIDTH_US;
                    current_chan++;
                    cycle_target = rest_us * CYCLES_PER_US - PRU1_CTRL.CYCLE;
                }
            }
        }
    }
}
