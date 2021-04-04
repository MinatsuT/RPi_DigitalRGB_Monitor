# Raspberry Pi デジタルRGBモニタ without X11
Raspberry PiとEZ-USB FX2LPを使用したデジタルRGB-HDMIコンバータです。X-Windowではなく、コンソール画面に全画面表示します。

## 必要なハードウェア
- Raspberry Pi 4/3/Zero (速度の問題で、Zeroだと少しノイズが出ます)
- EZ-USB FX2LP (AmazonやAliExpressで売っている青いボードでOKです。ただし、RDY0とRDY1のシルクが入れ替わっているものが多数見受けられます。詳しくは、後述します。)
- デジタルRGBの各信号を EZ-USB FX2LP に接続するケーブル

## EZ-USBのピンアサイン
```
PB0 --- デジタルRGBのB  
PB1 --- デジタルRGBのG  
PB2 --- デジタルRGBのR  
RB4 --- デジタルRGBのH-SYnc  
RB5 --- デジタルRGBのV-SYnc  
GND --- デジタルRGBのGND  
IFCLK --- デジタルRGBのColorClock(ピクセルクロック)  

RDY0/SLRD    --- VCC  
RDY1/DLWR    --- GND  
PA2/SLOE     --- VCC  
PA4/FIFOADR0 --- GND  
PA5/FIFOADR1 --- VCC  
```

## コンパイルのしかた
```
$ sudo apt install libusb-1.0-0-dev
$ git clone https://github.com/MinatsuT/RPi_DigitalRGB_Monitor.git
$ cd RPi_DigitalRGB_Monitor
$ make
```

## 実行のしかた
1. Raspberry Pi をCLI（コンソール画面）で起動します。X-Windowを使用している場合は、Alt+Ctrl+F2 でコンソール画面に切り替えられます。その場合、Alt+Ctrl+F1でX-Windowsに戻れます。
2. Raspberry Pi、EZ-USB FX2LP、デジタルRGBを出力するPCを接続します。
3. EZ-USB FX2LPとデジタルRGBを出力するPCの電源を入れます。
4. Raspberry PiがEZ-USB FX2LPを認識すると、デフォルトで *usbtest* ドライバ（カーネルモジュール）が読み込まれるので、これを外します。
```
$ sudo rmmod usbtest
```
5. スーパーユーザで、digital_rgb_display を実行します。
```
$ sudo ./digital_rgb_display 
```

## 楽に実行する方法
usbtestドライバはEZ-USB FX2LPを接続するたびに読み込まれるため、面倒です。そこで、以下の手順で、usbtest ドライバから自動的にEZ-USB FX2LPを切り離すことができます。
1. 以下の内容を、/etc/udev/rules.d/z70-usbfx2.rules に保存します。
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
2. udevルールを再読み込みします。
```
$ sudo udevadm control --reload-rules
```

さらに、自身のアカウントを plugdev に追加しておくと、スーパーユーザでなくても実行できるようになります。
```
$ sudo usermod -aG plugdev ログイン名
```

## EZ-USB FX2LPボードの注意！
よく出回っている、EZ-USB FX2LPの青いボードには、RDY0とRDY1のシルクが逆に印刷されているものが多数あります。シルクが読める方向からボードを見た場合、正しい配置は、  
「**左側**（チップから遠いほう）が**RDY0**」で、「**右側**（チップに近いほう）が**RDY1**」  
です。



