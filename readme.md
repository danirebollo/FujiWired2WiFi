FUJITSU HEAVY INDUSTRIES AIR CONDITIONER LIBRARY
# Introduction
This library is a ESP32 C++ library for controlling Fujitsu Heavy Industries Air Conditioners models xxxxxx. It is reverse enginereed as no official API is available. The library is based on the work of [this](), [this](), [this]() and [this]() projects.

# Protocol. Hardware layer
FUJITSU air conditioners use a half duplex (RX and TX on the same wire), LSB first, 500 baud UART signal with parity control to communicate with the remote controller. 
Like most of the shared buses, the bus is pulled up and controllers are meand only to pull the bus down (not putting any voltage on it to avoid short circuits) in an open collector or open drain mode.
There are three wires on the bus: GND, 15V and DATA. The GND is used as a reference voltage for the DATA signal, and DATA, when pulled high, is near 15V or at least 12V.

# Protocol. Data layer
Over the UART signal, the air conditioner sends and receives 8 bytes packets. 
This is an example of one packet:
FF 5F FF D7 EF FF FE FF   mSrc:   0, mDst:  32, onOff: 0, mType: 0, temp: 16 | cTemp: 0, acmode: 4, fanmode: 2, write: 0, login: 1, unk: 1, cP:1, uMgc:  0,  acError:0, id:    237
- first byte:
- second byte:
- third byte:
- fourth byte:
- fifth byte:
- sixth byte:
- seventh byte: allways 0xFF

# Library details


## Modes
- Slave mode: The library is able to control the air conditioner in slave mode replying to UNIT packets. This is the default mode.
- Master mode: The library is able to control the remote LCD in master mode sending packets to it.
- Slave mode with LCD attached: The library is able to control the air conditioner in slave mode and the remote LCD in master mode at the same time and in sync by using two different UART ports. Sync means that we can use both UNIT and LCD controller with the same latest updated parameters.
- LCD Read only mode: The library is able to control the air conditioner in slave mode and LCD sniffs the communication showing latest updated parameters. LCD board can't be used to control the air conditioner in this mode.

## WiFi control
The library exposes an API to controll the air conditioner over WiFi, in particular using Home Assistant.
When WiFi module is enabled the library will use WiFi as a new actor in the bus so now parameters can be read and transmit, using the most updated change of any actor as a reference.
The actors on the bus are:
- UNIT: the air conditioner
- LCD: the remote controller
- WiFi: the WiFi module

## API
- On/Off: turn on or off the air conditioner
- AC Mode: Auto, Cool, Dry, Fan, Heat
- Target temperature: 16 to 30 degrees
- Fan speed: 0(auto) to 4
- UNIT Error code: read errors from unit.
- LCD error code: send an error to the LCD so blink his power LED.
- Remote temperature: Temperature with UNIT try to reach. There are a temperature sensor on LCD controller, and can be used as remote temperature OR use the API to set the remote temperature to be used by UNIT.
- Economy mode: enable or disable economy mode.
- Swing mode (not available on all models): enable or disable swing mode.
- Swing step (not available on all models): set swing step.

- Remote humidity: with external sensor
- UNIT Power consumption: with external sensor
- EXTERIOR Power consumption: with external sensor
- Humidifier: with external sensor
- Power switch: with external sensor. AC can consumes power (as high as 1A) when off to keep the oil warm and avoid damage to the compressor. If unit will be off enough time (more than 5 min) it is recommended to turn off the power switch. Using a switch powering of the main bus supply the MCU needs to be powered externally (TODO: use GPIO to read voltage on ext port and avoid powering off the switch...).

## Cache
Slave mode needs to reply UNIT packets in a very small time frame (50ms), so it needs to cache the parameters to be able to fulfill the time frame.
There are CACHE_BUFFER_SIZE slots in the cache (10 by default) storing the received packet from the UNIT with his corresponding processed parameters reply, so MCU can reply the right processed parameter.

Master mode don't need to cache the parameters as it will always send the latest parameters and don't need respond to LCD.

# HARDWARE
The only needed part to be able to talk with the air conditioner is a 500 baud UART signal with parity control. But using the specific hardware described below will make the library more reliable, robust and easy to use, including the following features:
- 15V to 3V3 level shifter
- Bus write detection / inhibit to avoid collisions
- Power supply consumption measurement
- External temperature sensor
- External humidity sensor
- External power switch
- Easy to use connectors
- Error LED
- Mode selector to bypass the MCU and use the air conditioner as usual or put LCD in read-only mode

# DEBUG TOOLS
To get better understanding of the protocol and the library, there are some tools that can be used:
- Logic analyzer: to sniff the bus and see the packets. Set to 500 baud, 8N1, parity even.
- Oscilloscope: to see the signal and voltage levels.
- python script: included python script "reader.py". Connect ESP32 UART to the computer and write the assigned port in the py file (default is COM3). Run the script to decode the packets and see the parameters.

# Flash the MCU
To flash the MCU, load this library in PlatformIO and flash the MCU using the USB port. The library is tested with ESP32.