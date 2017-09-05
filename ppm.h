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
#ifndef BEAGLEBONE_PPM
#define BEAGLEBONE_PPM

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// --------------------------------------------------
// Interface

#define PPM_RDONLY         0x01
#define PPM_WRONLY         0x02
#define PPM_CONFIG_PINS    0x04
#define PPM_LOAD_FIRMWARE  0x08
#define PPM_NONBLOCKING    0x10

typedef struct _ppm_t ppm_t;

ppm_t* ppm_new(uint8_t flags);

int ppm_read(ppm_t* ppm, uint16_t* channels_out);

int ppm_write(ppm_t* ppm, uint16_t* channels_in);

void ppm_close(ppm_t* ppm);

// --------------------------------------------------
// Implementation

#define PRU_READ_DEVICE "/dev/rpmsg_pru30"
#define PRU_WRITE_DEVICE "/dev/rpmsg_pru31"
#define CHANNELS_SIZE 12 * sizeof(uint16_t)

struct _ppm_t {
    int fd;
    bool read_only;
};

ppm_t* ppm_new(uint8_t flags) {
    if ((flags & PPM_RDONLY) && (flags & PPM_WRONLY)) {
        fprintf(stderr, "Cannot open PPM in both read and write mode\n");
        return NULL;
    }

    int fd = open(
        (flags & PPM_RDONLY) ? PRU_READ_DEVICE : PRU_WRITE_DEVICE,
        O_RDWR);

    if (flags & PPM_NONBLOCKING) {
        int pru_flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, pru_flags & O_NONBLOCK);
    }

    ppm_t* ppm = (ppm_t*) malloc(sizeof(ppm_t));
    if (!ppm) {
        close(fd);
        return NULL;
    }

    ppm->fd = fd;
    ppm->read_only = (flags & PPM_RDONLY);

    return ppm;
}

int ppm_read(ppm_t* ppm, uint16_t* channels_out) {
    static uint8_t byte = 0;
    if (ppm->read_only) {
        // Send a read request (any message) to PRU to prompt it to write the channels back.
        if (write(fd, &byte, 1) != 1) {
            return -1;
        }

        return read(ppm->fd, channels_out, CHANNELS_SIZE);
    } else {
        return -1;
    }
}

int ppm_write(ppm_t* ppm, uint16_t* channels_in) {
    if (ppm->read_only) {
        return -1;
    } else {
        return write(ppm->fd, channels_in, CHANNELS_SIZE);
    }
}

void ppm_close(ppm_t* ppm) {
    close(ppm->fd);
}

#endif
