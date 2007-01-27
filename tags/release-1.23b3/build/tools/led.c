/*-
 * Copyright (c) 2006 Marcel Wiget
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/watchdog.h>

#define HELP "\n\
    led - Turn WRAP board LED's on and off\n\
\n\
Usage: led 1|2|3 0|1\n\
\n\
first number is the LED number starting from the left.\n\
second number: 0 for OFF, 1 for ON\n\
"

int main(int argc, char **argv)
{
    int fd;
    int led, onoff;
    unsigned u;

    if (argc < 3)
    {
        fprintf(stderr,HELP);
        exit(1);
    }

    led = atoi(argv[1]);
    onoff = atoi(argv[2]);

    if (led < 1 || led > 3 || onoff < 0 || onoff > 1)
    {
        fprintf(stderr,HELP);
        exit(1);
    }

    if (1 == led)
    {
        u = WD_LED1; 
    } else if (2 == led) 
    {
        u = WD_LED2; 
    } else 
    {
        u = WD_LED3; 
    }
    u |= (onoff) ? WD_LED_ON : WD_LED_OFF;

    fd = open("/dev/" _PATH_WATCHDOG, O_RDWR);
    ioctl(fd, WDIOCPATPAT, &u);

    exit (0);
}
