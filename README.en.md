# Raspberry Pi Digital-RGB monitor without X11
Digital-RGB to HDMI converter using Raspberry Pi and EZ-USB FX2LP without X11.

## Required hardware
- Raspberry Pi 4/3/Zero (due to speed issues, RPi Zero will make a little noise).
- EZ-USB FX2LP development board(The blue boards sold on Amazon and AliExpress are OK. However, you will find many of them with the RDY0 and RDY1 silks swapped. See below for details.)
- Cables to connect each digital RGB signal to the EZ-USB FX2LP.

## EZ-USB Pin Assignment
Between EZ-USB and Digital-RGB:
```
EZ-USB     Digital-RGB
   PB0 --- Blue
   PB1 --- Green
   PB2 --- Red
   RB3 --- H-SYnc  
   RB4 --- V-SYnc  
   GND --- GND
 IFCLK --- ColorClock(PixelClock)  
```

Inside the EZ-USB:
```
RDY0/SLRD    --- VCC  
RDY1/SLWR    --- GND  
PA2/SLOE     --- VCC  
PA4/FIFOADR0 --- GND  
PA5/FIFOADR1 --- VCC  
```

## How to compile
```
$ sudo apt install libusb-1.0-0-dev
$ git clone https://github.com/MinatsuT/RPi_DigitalRGB_Monitor.git
$ cd RPi_DigitalRGB_Monitor
$ make
```

## How to use
1. Start the Raspberry Pi in the CLI (console screen). If you are using X-Window, you can switch to the console screen by pressing Alt+Ctrl+F2. In that case, you can return to X-Windows with Alt+Ctrl+F1.
2. Connect Raspberry Pi, EZ-USB FX2LP, and PC that outputs digital RGB.
3. Turn on the EZ-USB FX2LP and the PC that outputs digital RGB.
4. When the Raspberry Pi recognizes the EZ-USB FX2LP, it will load the *usbtest* driver (kernel module) by default, so remove it.
```
$ sudo rmmod usbtest
```
5. As a super user, run digital_rgb_display.  
**Note**: Before starting the program, the PC connected via the digital RGB should be turned ON. This is because the ColorClock of the digital RGB is used for the clock source of the EZ-USB FX2LP.
```
$ sudo ./digital_rgb_display 
```

## How to run it easily
The usbtest driver is cumbersome because it is loaded every time you connect the EZ-USB FX2LP. So, you can automatically disconnect EZ-USB FX2LP from the usbtest driver by the following steps:
1. Save the following to "/etc/udev/rules.d/z70-usbfx2.rules".
```
# USBFX2 development board udev rules which do the following:
# * allow access for users in group "plugdev"
# * unbind the FX2 peripheral from usbtest driver
# 10-2010 by Vesa Solonen. Tested on plain CY7C68013A on Ubuntu 10.04.
# Device properties can be found via dmesg (driver and device number) and udevadm info:
# udevadm info -a -p /sys/bus/usb/drivers/usbtest/1-3\:1.0
# Reference: http://www.reactivated.net/writing_udev_rules.html
SUBSYSTEM=="usb", ACTION=="add", ATTR{idVendor}=="04b4", ATTR{idProduct}=="8613", GROUP="plugdev", MODE="0660"
SUBSYSTEM=="usb", ACTION=="add", ATTRS{idVendor}=="04b4", ATTRS{idProduct}=="8613", DRIVER=="usbtest", RUN+="/bin/sh -c 'echo -n %k> %S%p/driver/unbind'"
```
2. Reload the udev rules.
```
$ sudo udevadm control --reload-rules
```

Further more, if you add your own account to plugdev, you can run it even if you are not a super user.
```
$ sudo usermod -aG plugdev Your-Login-Name
```

## Note on a cheap EZ-USB FX2LP board!
There are many EZ-USB FX2LP blue boards with the RDY0 and RDY1 silks printed backwards. If you look at the board from the direction you can read the silk, the correct placement is:  
"**left side** (farther from the chip) is **RDY0**" and "**right side** (closer to the chip) is **RDY1**".  
