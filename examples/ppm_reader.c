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
#include <stdio.h>
#include <time.h>
#include "../ppm.h"

int main(int argc, char **argv) {
    uint16_t channels[12];
    ppm_t* ppm = ppm_new(PPM_RDONLY);
    if (ppm == NULL) {
        exit(1);
    }

    uint64_t elapsed_ms;
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);

    for (int loop=0; ; loop++) {
        if (ppm_read(ppm, channels) < 0) {
            fprintf(stderr, "fail\n");
            exit(1);
        }

        clock_gettime(CLOCK_MONOTONIC_RAW, &now);
        elapsed_ms = (now.tv_sec - start.tv_sec) * 1000 +
            (now.tv_nsec - start.tv_nsec) / 1000000;

        printf("%d frames read at an average of %.2fms latency\n",
               loop+1, ((float) elapsed_ms) / (loop+1));

        for (int i=0; i<12; i++) {
            printf("channel[%d] = %d\n", i, channels[i]);
        }
        printf("-----------------------------------------------\n");
    }
}
