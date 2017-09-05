Library and PRU firmware to read/write PPM protocol on BeagleBone
black. PPM is commonly used in sending input channel data between RC
transmitters, receivers, servos, and flight controllers.

Example usage:

```c

#include "ppm.h"

int main(int argc, char **argv) {
    uint16_t channels[12];
    ppm_t* ppm = ppm_new(PPM_WRONLY);
    if (ppm == NULL) {
        exit(1);
    }

    int i, loop;
    for (loop = 0; ; loop++) {
        for (i=0; i<12; i++) {
            channels[i] = 1000 + 10 * (i+1) + loop;
        }

        ppm_write(ppm, channels);

        sleep(1);
    }
}
```

TODO: add build and install instructions for PRU firmware.

--------------------------------------------------
Notes:

Q: can we package this in such a way that when you use this library, you
can easily specify which PRU to run the decoder/encoder on, and which
pin is used for as gpio?

perhaps a top-level make command like:

    PRU=0 GPIO=P8_45 make decoder

output would then be in gen/decoder_pru0_p8_45.out

then you could use the library like:

     ppm_new(PRU_0, "P8_45", PPM_WRONLY)

Q: do we want to include the .out file in the ppm_new constructor so
that we can load it up and reboot it every time we start using the
library?

--------------------------------------------------

References:
* https://quadmeup.com/generate-ppm-signal-with-arduino/
* http://fpvlab.com/forums/showthread.php?27271-PPM-frame-rate
* http://rcarduino.blogspot.com/2012/11/how-to-read-rc-receiver-ppm-stream.html
* http://www.mftech.de/ppm_en.htm
* https://github.com/ZeekHuge/BeagleScope