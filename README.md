RdSpi
=====

Si4703 based RDS scanner for Raspberry Pi using Si4703S-B16 breakout board PL102RT-S V1.5

![PL102RT-S V1.5](http://1.bp.blogspot.com/-j91XFG7MIac/U4t7Ey8lmWI/AAAAAAAAATA/-J92Tfr5Ej0/s1600/pl102rt-s-v15.png)

Installation
------------

Connect PL102RT-S V1.5 breakout to Rpi:

| RPi      | PL102RT-S |
| -------- |----------:|
| SDA      | SDIO      |
| SCL      | SCLK      |
| #23 (GP4)| RST       |
| GND      | GND       |
|3.3V      | Vd        |

If in addition to RDS scanning you want to listen to radio audio then connect
whatever amplifier you have to LOUT/ROUT. [Adafruit MAX98306](http://www.adafruit.com/products/987) works just fine.

Make RdSpi. It accepts one command at a time:

* **_cmd_** - run in interactive command mode
* **_reset_** - resets and powers up Si4703, dumps register map while resetting
* **_power on|down_** - powers Si4703 up or down
* **_dump_** - dumps Si4703 register
* **_spacing kHz_** - sets 200, 100, or 50 kHz spacing
* **_scan (mode)_** - scans for radio stations, mode can be specified 1-5, see [AN230](http://www.silabs.com/Support%20Documents/TechnicalDocs/AN230.pdf), Table 23. Summary of Seek Settings
* **_spectrum_** - scans full FM band and prints RSSI
* **_seek up|down_** - seeks to the next/prev station
* **_tune freq_**  - tunes to specified FM frequency, for example `rdspi tune 9500` or `rdspi tune 95.00` or `rdspi tune 95.` to tune to 95.00 MHz
* **_rds on|off|verbose_** - sets RDS mode, on/off for RDSPRF, verbose for RDSM
* **_rds [gt G] [time T] [log] _** - scan for RDS messages. Use to _gt_ specify RDS Group Type to scan for, for example 0 for basic tuning and switching information. Use _time_ to specify timeout T in seconds. T = 0 turns off timeout. Use _log_ to scroll output instead on using one-liners. 
* **_rds_** - scan for complete RDS PS and Radiotext messages with default 15 seconds timeout
* **_volume 0-30_** - set audio volume, 0 to mute
* **_set register=value_** - set specified register

It is better to start with `reset` :)

```
rdspi reset
rdspi tune 95.00
rdspi volume 10
rdspi set volext=1
rdspi dump
```

To disable output use ```--silent``` as the last command line parameter:
```
rdspi reset --silent
rdspi tune 9500  --silent
```

Screenshots
-----------

* [rdspi dump](http://3.bp.blogspot.com/-OXuzT8qIl9Y/U4uHJIeWVyI/AAAAAAAAATQ/cm2Y-9AsPI0/s1600/dump.png)
* [rdspi scan](http://4.bp.blogspot.com/-w3Rr9ScBuhA/U4uHeOIE43I/AAAAAAAAATY/xRO8Dcd-KSw/s1600/scan.png)
* [rdspi spectrum](http://1.bp.blogspot.com/-7OW7MaMvY_M/U4uHlLo7ZaI/AAAAAAAAATg/le6EdWwt_OI/s1600/spectrum.png)
* [rdspi rds log](http://1.bp.blogspot.com/-Lwb6mZEmLF4/U4uH0sbmRTI/AAAAAAAAATo/-339yycuW_E/s1600/rds.png)

Links
-----

* [Si4702/03 FM Radio Receiver ICs](http://www.silabs.com/products/audio/fmreceivers/pages/si470203.aspx)
* [Si4703S-C19 Data Sheet](https://www.sparkfun.com/datasheets/BreakoutBoards/Si4702-03-C19-1.pdf)
* Si4703S-B16 Data Sheet: *Not on SiLabs site anymore, [google](https://www.google.com/search?q=Si4703+B16) for it*
* Si4700/01/02/03 Firmware Change List, AN281: *Not on SiLabs site anymore, [google](https://www.google.com/search?q=Si4700%2F01%2F02%2F03+Firmware+Change+List%2C+AN281) for it*
* [Using RDS/RBDS with the Si4701/03](http://www.silabs.com/Support%20Documents/TechnicalDocs/AN243.pdf)
* [Si4700/01/02/03 Programming Guide](http://www.silabs.com/Support%20Documents/TechnicalDocs/AN230.pdf)
* RBDS Standard: ftp://ftp.rds.org.uk/pub/acrobat/rbds1998.pdf
