//
// firmware for Cypress EZ-USB FX2LP
// 8bit Synchronous Slave FIFO
//
// 4-Apr-2021 by Minatsu (@tksm372)
//
#include "Fx2.h"
#include "fx2regs.h"
#include "syncdly.h"

void Initialize() {
    // ----------------------------------------------------------------------
    // CPU Clock
    // ----------------------------------------------------------------------
    // bit7:6 -
    // bit5   1=PortC RD#/WR# Strobe enable
    // bit4:3 00=12MHz, 01=24MHz, 10=48MHz, 11=reserved
    // bit2   1=CLKOUT inverted
    // bit1   1=CLKOUT enable
    // bit0   1=reset
    CPUCS = 0x10; // 0b0001_0000; 48MHz
    SYNCDELAY;

    // ----------------------------------------------------------------------
    // Interface Config
    // ----------------------------------------------------------------------
    // bit7   1=Internal clock, 0=External
    // bit6   1=48MHz, 0=30MHz
    // bit5   1=IFCLK out enable
    // bit4   1=IFCLK inverted
    // bit3   1=Async, 0=Sync
    // bit2   1=GPIF GSTATE out enable
    // bit1:0 00=Ports, 01=Reserved, 10=GPIF, 11=Slave FIFO
    IFCONFIG = 0x03; // 0b0000_0011; External clock, Sync, Slave FIFO
    SYNCDELAY;

    // ----------------------------------------------------------------------
    // Chip Revision Control
    // ----------------------------------------------------------------------
    REVCTL = 0x03; // Recommended setting.
    SYNCDELAY;

    // ----------------------------------------------------------------------
    // EP Config
    // ----------------------------------------------------------------------
    // bit7   1=Valid
    // bit6   1=IN, 0=OUT
    // bit5:4 00=Invalid, 01=Isochronous, 10=Bulk(default), 11=Interrupt
    // bit3   1=1024bytes buffer(EP2,6 only), 0=512bytes
    // bit2   -
    // bit1:0 00=Quad, 01=Invalid, 10=Double, 11=Triple
    EP2CFG &= 0x7f; // disable
    SYNCDELAY;
    EP4CFG &= 0x7f; // disable
    SYNCDELAY;
    EP6CFG = 0xe0; // 0b1110_0000; Bulk-IN, 512bytes Quad buffer
    SYNCDELAY;
    EP8CFG &= 0x7f; // disable
    SYNCDELAY;

    // ----------------------------------------------------------------------
    // FIFO Reset
    // ----------------------------------------------------------------------
    FIFORESET = 0x80; // NAK all transfer
    SYNCDELAY;
    FIFORESET = 0x86; // Reset EP6 FIFO
    SYNCDELAY;
    FIFORESET = 0x00; // Resume
    SYNCDELAY;

    // ----------------------------------------------------------------------
    // EP FIFO Config
    // ----------------------------------------------------------------------
    // bit7   -
    // bit6   1=IN Full Minus One
    // bit5   1=OUT Empty Minus One
    // bit4   1=AUTOOUT
    // bit3   1=AUTOIN
    // bit2   1=Zero length IN Packerts enable
    // bit1   -
    // bit0   1=16bit wide, 0=8bit wide
    EP6FIFOCFG = 0x0e; // 0b0000_1100; Auto-IN, 8bit
    SYNCDELAY;

    // ----------------------------------------------------------------------
    // Auto IN Length
    // ----------------------------------------------------------------------
    EP6AUTOINLENH = (512 >> 8);
    SYNCDELAY;
    EP6AUTOINLENL = 0;
    SYNCDELAY;
}

void main() {
    Initialize();

    for (;;) {
    }
}