# Hydrolab Sensor

The hydrolab-compatible LoRaWAN sensor. This repository contains the
code needed to flash into the sensor the HydroLab-protocol-compatible code.

Keep reading to be able to flash the code. We suppose you have the
[iTIC board](https://itic.cat).

## Prerequisites

You need:
- An Arduino board.
- Arduino IDE installed and working.
- A jumper cable between the `MISO` and `M_PROG` pins of the board.
- The ISCP connection between the board and the Arduino. There are plenty of
  tutorials on the Internet, just pick one. **Warning:** the board works with
  3V3, so remember to use voltage dividers where needed (or use a 3V3 board).

## Flashing ArduinoISP

Open Arduino IDE and go to File->Examples->11.ArduinoISP->ArduinoISP.
Select the correct Arduino board and port. Select the default programmer:
*AVRISP mkII*, and flash the program to the Arduino.

Now you have a Arduino that will be able to inject code to the custom board.

## Setting up project

Copy all the contents from the `libraries` directory to your Arduino IDE
libraries path. For snap-installed ArduinoIDE the path will be
`~/snap/arduino/current/Arduino/libraries/`.

Go to ArduinoIDE and load the `sensor` directory (the `sensor/sensor.ino` file).
Change the LoRaWAN session credentials to yous before compiling!

Once done, you should be able to compile (*validate* button) the project
successfully.

> **Why can't i just download the official libraries from the official UI?**
> 
> The `ArduinoLMIC` library is a very resource-demanding library, and our
> target board is very resource-limited. That's why we manually removed some of
> its features (that weren't useful for our project) in order to light things
> up.
> 
> That library needs also to be configured with the specific region (EU in our
> case) and this needs to be done in the library files too.
> 
> The other libraries can be manually downloaded from the Arduino official UI as
> they haven't been modified, but for the sake of simplicity and ease of
> installation we've included everything you need in this repository.

## Flashing

Select the board **Arduino Pro or Pro Mini**. Then, on tools->processor, you
can select the board's frequency and voltage: 3.3V and 8MHz.

Once this is done, go to Sketch->Upload using programmer. Flashing the
code through ISP is a little longer than doing it the standard way, but as we
need to do this only one time it will be ok.

## Running

Change the jumper that was at `MISO`<->`M_PROG` to `MISO`<->`M_LORA`.

Remove the ISP cables, press the OFF button and then the ON button. If you can
see a Red LED and after some time a Green LED, it worked! Congratulations!

# License and contributing

The files created for this project are under the Apache 2.0 license.
The libraries used may have other licenses, check their LICENSE or README files.

If you want to contribute to the libraries do it in their own repositories.
If you want to contribute to this specific project, just create a PR!
