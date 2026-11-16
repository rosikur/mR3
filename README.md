# mR3

## Before we start

I'm so so incredibly Sorry but the firmware was made by an artifical intelligence. This was the result of pixl.hackclub.com shutting down in which I had to finish my Project as soon as possible

## Introduction

Hey Guys! This is my first Music Player Project. The Goal is to learn how to wire
a DAC(Digital Audio Convertor) without Signal Lose. An Amplifier without Noise.
How to power an Stereo Speaker System. How to cnc machine the chassis and how to
fit everything in a compact chassis with a display.

![cad with text](https://cdn.hackclub.com/01a1e4fc-ffb1-7412-ad6b-85f78517e2f3/untitled.jpg)

## Components

# ICs:

- TPA6132ARTE
- PAM8403D
- PCM5120A
- ESP32-S3-WROOM-1
- TPS63802DLAR
- BQ25895RTW
- 2.8" Display(See BOM and PINS MAKE SURE YOU BUY one with the same PIN count)

# Periphals

- 3x THT SW Tactile SPST Angeled
- 6x THT SW Push 6mm H5mm
- 1x USB c 2.o Receptaple
- 1x Audio Jack SwitchTR
- 1x 3.7 V Battary Pack (Must be with BMS)
- SD Card Holder (Wuerth)
- 3w Speakers (2)
- Soldering Iron

## How to build it?

# PCB

Firstly take the gerber.zip and upload them to the PCB manufacturer of your choosing, I prefer JLCPCB as i can use the bom in their daughter company lscs to buy the components.
I really recommend to order a stencil with your order because you'll be miserable soldering IC's

Secondly go to LSCS with the bom file and order everything with their MOQ (Minimum Order Quantity) I really recommend to get extra Components 0805s are really common for lots of Projects, if you follow my projects, you can be sure that we'll be using them.

Thirdly, get some solder paste and a Heatplate if you havn't yet else your closest Hackerspace will have some (Props to the C3PB and CCC)

Fourth, open the PCB on your Laptop and grab tweezers and the pcb
put the stencil on (smh) and spread the paste with an old Credit Card. Make sure to put every componet correctly ESPECIALLY THE BATTERY and the IC's (ESP32 are hella expensive.)

Fifth, select a side which will be easier to solder (the back in our case) add everything and put them on the Heatplate or a reflow oven

Lastly make sure to put the battery in correctly THIS IS VERY IMPORTANT. Add the speakers correctly so they dont play in reverse. that should be about it for the pcb onto the case!

# PCB images

![front](https://user-cdn.hackclub-assets.com/01a1e506-079d-7ce6-87b5-ea25bf6bda13/image.png)
![back](https://user-cdn.hackclub-assets.com/01a1e504-2ec7-71bc-8c40-efebca243627/image.png)
![kicad](https://user-cdn.hackclub-assets.com/01a1e500-09fc-71a2-80eb-f29b66e04aaf/image.png)

# Case

3d print it CNC it idc, i will be uploading the STL , the 3mf the STEP and the cnc file.
I recommend Prototyping with 3d Print and CNC as a finished Product. I will be Posting both results soon!

The Case is now almost finished, I only need to add the buttons,
Here is how it turned out:
![An CAD Model of an MP3 Player](https://cdn.hackclub.com/01a1d5a2-138b-7a92-a455-95550f6b7a9c/img_2143.png)

# Firmware

Install esp tool or plattform IO press the boot button and plug it into your Computer if you are using Plattform IO you'll have to compile it first ,for esp tool use the ready .bin files :D once its sent to the ESP its ready to use have fun
(Obviously assemble it first)

# BOM

The complete bill of materials is also available as [`bom.csv`](bom.csv).

| Component              | Designators                                             | Footprint                                                    | Qty. | Supplier                                                                                                                         |
| ---------------------- | ------------------------------------------------------- | ------------------------------------------------------------ | ---: | -------------------------------------------------------------------------------------------------------------------------------- |
| 0.47uF                 | C1, C2                                                  | `C_0805_2012Metric_Pad1.18x1.45mm_HandSolder`                |    2 | [Buy](https://www.tme.eu/de/details/cl21b474kafnnng/kondensatoren-mlcc-smd/samsung/)                                             |
| 2.2uF                  | C3, C4, C8, C9                                          | `C_0805_2012Metric_Pad1.18x1.45mm_HandSolder`                |    4 | [Buy](https://www.tme.eu/de/details/0805b225k160ct/kondensatoren-mlcc-smd/walsin/)                                               |
| 0.1uF                  | C5, C6, C14, C15, C16, C19                              | `C_0805_2012Metric_Pad1.18x1.45mm_HandSolder`                |    6 | [Buy](https://www.tme.eu/de/details/0805b104k500ct/kondensatoren-mlcc-smd/walsin/)                                               |
| 1uF                    | C7, C10, C11, C12, C13, C21, C22, C23, C24, C31         | `C_0805_2012Metric_Pad1.18x1.45mm_HandSolder`                |   10 | [Buy](https://www.tme.eu/de/details/0805b105k250ct/kondensatoren-mlcc-smd/walsin/)                                               |
| 10uF                   | C17, C18, C26                                           | `C_0805_2012Metric_Pad1.18x1.45mm_HandSolder`                |    3 | [Buy](https://www.tme.eu/de/details/c0805c106k9pac/kondensatoren-mlcc-smd/kemet/c0805c106k9pactu/)                               |
| 22uF                   | C20, C28, C29, C32, C33, C34                            | `C_0805_2012Metric_Pad1.18x1.45mm_HandSolder`                |    6 | [Buy](https://www.tme.eu/de/details/jmk212abj226mg-t/kondensatoren-mlcc-smd/taiyo-yuden/)                                        |
| 47nF                   | C27                                                     | `C_0805_2012Metric_Pad1.18x1.45mm_HandSolder`                |    1 | [Buy](https://www.tme.eu/de/details/0805b473k500cg/kondensatoren-mlcc-smd/walsin/)                                               |
| 4.7uF                  | C30                                                     | `C_0805_2012Metric_Pad1.18x1.45mm_HandSolder`                |    1 | [Buy](https://www.tme.eu/de/details/0805x475k6r3ct/kondensatoren-mlcc-smd/walsin/)                                               |
| LED                    | D1                                                      | `LED_0805_2012Metric_Pad1.15x1.40mm_HandSolder`              |    1 | [Buy](https://www.tme.eu/de/details/of-smd2012r/led-dioden-smd-bunt/optoflash/)                                                  |
| Micro SD card          | J1                                                      | `microSD_HC_Wuerth_693072010801`                             |    1 | [Buy](https://www.mouser.de/en/ProductDetail/Wurth-Elektronik/693072010801?qs=7gQLVZk5cPmw4uU1mAcUFg%3D%3D)                      |
| Audio jack             | J2                                                      | `Jack_3.5mm_CUI_SJ1-3515N_Horizontal`                        |    1 | [Buy](https://www.tme.eu/de/details/sj13513/klinken-steckverbinder/same-sky-r/sj1-3513/)                                         |
| 9-pin socket _(DNP)_   | J3                                                      | `PinHeader_1x09_P2.54mm_Vertical`                            |    1 | DNP                                                                                                                              |
| USB-C receptacle       | J4                                                      | `USB_C_Receptacle_Amphenol_12401548E4-2A`                    |    1 | [Buy](https://www.mouser.de/en/ProductDetail/Amphenol-Commercial-Products/12401598E42A?qs=367PjNmvCmllVMBVyIyS3w%3D%3D)          |
| 2-pin connector        | J5                                                      | `JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal`                    |    1 | [Buy](https://www.tme.eu/de/details/s2b-xh-a-1/signalsteckverbinder-raster-2-50mm/jst/s2b-xh-a-1-lf-sn/)                         |
| 2.2uH inductor         | L1                                                      | `L_2512_6332Metric_Pad1.52x3.35mm_HandSolder`                |    1 | [Buy](https://www.mouser.de/en/ProductDetail/Bourns/SRP2512-2R2M?qs=sGAEpiMZZMukHu%252BjC5l7YdTJBteetShHxC51KzP5FBA%3D)          |
| 0.47uH inductor        | L2                                                      | `L_2512_6332Metric_Pad1.52x3.35mm_HandSolder`                |    1 | [Buy](https://www.mouser.de/en/ProductDetail/652-SRP2512-R47M)                                                                   |
| Speaker                | LS1, LS2                                                | `JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal`                    |    2 | [Buy](https://a.aliexpress.com/_EJix1bo)                                                                                         |
| 10k resistor           | R1, R2, R4, R5, R6, R7, R9, R10, R13, R14, R16, R22–R33 | `R_0805_2012Metric_Pad1.20x1.40mm_HandSolder`                |   23 | [Buy](https://www.tme.eu/de/details/wr08x1002ftl/smd-widerstaende/walsin/)                                                       |
| 5.1k resistor          | R3, R12                                                 | `R_0805_2012Metric_Pad1.20x1.40mm_HandSolder`                |    2 | [Buy](https://www.tme.eu/de/details/wr08x5101ftl/smd-widerstaende/walsin/)                                                       |
| 1k resistor            | R8, R11                                                 | `R_0805_2012Metric_Pad1.20x1.40mm_HandSolder`                |    2 | [Buy](https://www.tme.eu/de/details/wr08x102jtl/smd-widerstaende/walsin/wr08x102-jtl/)                                           |
| 6.8k resistor          | R15                                                     | `R_0805_2012Metric_Pad1.20x1.40mm_HandSolder`                |    1 | [Buy](https://www.tme.eu/de/details/wr08x6801ftl/smd-widerstaende/walsin/)                                                       |
| 2.2k resistor          | R17                                                     | `R_0805_2012Metric_Pad1.20x1.40mm_HandSolder`                |    1 | [Buy](https://www.tme.eu/de/details/wr08x222jtl/smd-widerstaende/walsin/wr08x222-jtl/)                                           |
| 260 ohm resistor       | R18                                                     | `R_0805_2012Metric_Pad1.20x1.40mm_HandSolder`                |    1 | [Buy](https://www.tme.eu/de/details/rc0805fr-07261rl/smd-widerstaende/yageo/)                                                    |
| 100k resistor          | R19                                                     | `R_0805_2012Metric_Pad1.20x1.40mm_HandSolder`                |    1 | [Buy](https://www.tme.eu/de/details/wr08x1003ftl/smd-widerstaende/walsin/)                                                       |
| 91k resistor           | R20                                                     | `R_0805_2012Metric_Pad1.20x1.40mm_HandSolder`                |    1 | [Buy](https://www.mouser.de/en/ProductDetail/Panasonic-Industry/ERJ-6ENF9102V?qs=sGAEpiMZZMtlubZbdhIBIOIpaOjkYUZGTcw3DF91WWo%3D) |
| 511k resistor          | R21                                                     | `R_0805_2012Metric_Pad1.20x1.40mm_HandSolder`                |    1 | [Buy](https://www.tme.eu/de/details/smd0805-511k-1%25/smd-widerstaende/royalohm/0805s8f5113t5e/)                                 |
| Push button            | SW1–SW5, SW9                                            | `SW_PUSH_6mm_H5mm`                                           |    6 | [Buy](https://www.tme.eu/de/details/b3f-1000/mikroschalter-tact/aratas-formerly-omron-components/)                               |
| Angled tactile button  | SW6–SW8                                                 | `SW_Tactile_SPST_Angled_PTS645Vx31-2LFS`                     |    3 | [Buy](https://www.tme.eu/de/details/tacta-64n-f/mikroschalter-tact/ninigi/)                                                      |
| TPA6132A2RTE           | U1                                                      | `WQFN-16-1EP_3x3mm_P0.5mm_EP1.6x1.6mm_ThermalVias`           |    1 | [Buy](https://www.mouser.de/en/ProductDetail/595-TPA6132A2RTET)                                                                  |
| PAM8403D               | U2                                                      | `SOIC-16_3.9x9.9mm_P1.27mm`                                  |    1 | [Buy](https://www.mouser.de/en/ProductDetail/621-PAM8403DR-H)                                                                    |
| PCM5102A               | U3                                                      | `TSSOP-20_4.4x6.5mm_P0.65mm`                                 |    1 | [Buy](https://www.mouser.de/en/ProductDetail/595-PCM5102APWR)                                                                    |
| BQ25895RTW             | U5                                                      | `Texas_RTW_WQFN-24-1EP_4x4mm_P0.5mm_EP2.7x2.7mm_ThermalVias` |    1 | [Buy](https://www.mouser.de/en/ProductDetail/595-BQ25895RTWR)                                                                    |
| TPS63802DLAR           | U6                                                      | `DLA0010A`                                                   |    1 | [Buy](https://www.mouser.de/en/ProductDetail/595-TPS63802DLAR)                                                                   |
| ESP32-S3-WROOM-1       | U7                                                      | `ESP32-S3-WROOM-1`                                           |    1 | [Buy](https://www.mouser.de/en/ProductDetail/356-ESP32S3WROM1N4R8)                                                               |
| Battery                | x                                                       | `LIPo Battary`                                               |    1 | [Buy](https://a.aliexpress.com/_ExininM)                                                                                         |
| Display _(check pins)_ | x                                                       | `TFT Disp`                                                   |    1 | [Buy](https://a.aliexpress.com/_EHwSQO6)                                                                                         |
