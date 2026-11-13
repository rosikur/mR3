# mR3

## Before we start

I'm so so incredibly Sorry but the firmware was made by an artifical intelligence. This was the result of pixl.hackclub.com shutting down in which I had to finish my Project as soon as possible

## Introduction

Hey Guys! This is my first Music Player Project. The Goal is to learn how to wire
a DAC(Digital Audio Convertor) without Signal Lose. An Amplifier without Noise.
How to power an Stereo Speaker System. How to cnc machine the chassis and how to
fit everything in a compact chassis with a display.

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

# Case

3d print it CNC it idc, i will be uploading the STL , the 3mf the STEP and the cnc file.
I recommend Prototyping with 3d Print and CNC as a finished Product. I will be Posting both results soon!

The Case is now almost finished, I only need to add the buttons,
Here is how it turned out:
![An CAD Model of an MP3 Player](https://cdn.hackclub.com/01a1d5a2-138b-7a92-a455-95550f6b7a9c/img_2143.png)

# Firmware

Install esp tool or plattform IO press the boot button and plug it into your Computer if you are using Plattform IO you'll have to compile it first ,for esp tool use the ready .bin files :D once its sent to the ESP its ready to use have fun
(Obviously assemble it first)
