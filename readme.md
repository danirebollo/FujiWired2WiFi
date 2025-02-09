A/C (Air conditioner) library for wired half-duplex UART based protocols like the used in the Fujitsu ACY100UIA-LL (This bus have different names: RWB remote controller bus, RAC, VRF...). Maybe Daykin AC, GENERAL AC and HIYASU AC can be compatible attending at the compatibility list of [this](https://www.intesis.com/products/ac-interfaces/fujitsu-gateways/fujitsu-knx-inputs-vrf-fj-rc-knx-1i) comertial gateway.

# Introduction
This library is a ESP32 C++ library for controlling selected air conditioners. It is reverse enginereed as no official API is available. The library is based on the work of [FujiHeatPump](https://github.com/unreality/FujiHeatPump), [fuji-iot](https://github.com/jaroslawprzybylowicz/fuji-iot/), [FujiHK](https://github.com/unreality/FujiHK)...

## Tested models
- Fujitsu ACY100UIA-LL: Works perfectly. This is the main model used for development.

# Protocol. Hardware layer
FUJITSU air conditioners use a half duplex (RX and TX on the same wire), LSB first, 500 baud UART signal with parity control to communicate with the remote controller. 
Like most of the shared buses, the bus is pulled up and controllers are meand only to pull the bus down (not putting any voltage on it to avoid short circuits) in an open collector or open drain mode.
There are three wires on the bus: GND, 15V and DATA. The GND is used as a reference voltage for the DATA signal, and DATA, when pulled high, is near 15V or at least 12V.

# Protocol. Data layer
Over the UART signal, the air conditioner sends and receives 8 bytes packets. 

This is an example of one packet:
```FF 5F FF D7 EF FF FE FF```

```mSrc:   0, mDst:  32, onOff: 0, mType: 0, temp: 16 | cTemp: 0, acmode: 4, fanmode: 2, write: 0, login: 1, unk: 1, cP:1, uMgc:  0,  acError:0, id:    237```
- first byte:   0b11111111  source address
- second byte:  0b00111000  ??|messagetype|writebit|???
- third byte:   0b11111111  error|fan|mode|enabled
- fourth byte:  0b11111111  economy|temperature
- fifth byte:   0b11110110  updatemagic|?|swing|swingstep|?
- sixth byte:   0b01111111  ?|controllerTemp|controllerPresent
- seventh byte: 0b00000000  ?|?|?|?|?|?|?|? Show maintenance mode icon when value is between about 60 and about 131 and about 196 to about 1

UNIT address is 0, LCD address is 32. 

We need to act like LCD for the UNIT (slave) and like UNIT for the LCD (master).

lots of sniffed messages: https://github.com/jaroslawprzybylowicz/fuji-iot/blob/master/protocol/fuji_ac_protocol_handler_test.cc

# Software
## Library Modes
- Slave mode: The library is able to control the air conditioner in slave mode replying to UNIT packets. This is the default mode.
- Master mode: The library is able to control the remote LCD in master mode sending packets to it.
- Slave mode with LCD attached: The library is able to control the air conditioner in slave mode and the remote LCD in master mode at the same time and in sync by using two different UART ports. Sync means that we can use both UNIT and LCD controller with the same latest updated parameters.
- LCD Read only mode: The library is able to control the air conditioner in slave mode and LCD sniffs the communication showing latest updated parameters. LCD board can't be used to control the air conditioner in this mode.

## Cache
Slave mode needs to reply UNIT packets in a very small time frame (50ms), so it needs to cache the parameters to be able to fulfill the time frame.
There are CACHE_BUFFER_SIZE slots in the cache (10 by default) storing the received packet from the UNIT with his corresponding processed parameters reply, so MCU can reply the right processed parameter.

Master mode don't need to cache the parameters as it will always send the latest parameters and don't need respond to LCD.

## WiFi control [TODO]
The library exposes an API to controll the air conditioner over WiFi, in particular using Home Assistant.
When WiFi module is enabled the library will use WiFi as a new actor in the bus so now parameters can be read and transmit, using the most updated change of any actor as a reference.
The actors on the bus are:
- UNIT: the air conditioner
- LCD: the remote controller
- WiFi: the WiFi module [TODO]

### How to use with Home assistant (HASSIO) [TODO]
#### Adding ESPHOME custom component [TODO]
#### Using MQTT [TODO]

## Web API [TODO]
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

- Remote humidity: with external sensor [TODO]
- UNIT Power consumption: with external sensor [TODO]
- EXTERIOR Power consumption: with external sensor [TODO]
- Humidifier: with external sensor [TODO]
- Power switch: with external sensor. AC can consumes power (as high as 1A) when off to keep the oil warm and avoid damage to the compressor. If unit will be off enough time (more than 5 min) it is recommended to turn off the power switch. Using a switch powering of the main bus supply the MCU needs to be powered externally (TODO: use GPIO to read voltage on ext port and avoid powering off the switch...). [TODO]

# Flash the MCU
To flash the MCU, load this library in PlatformIO and flash the MCU using the USB port. The library is tested with ESP32.

# HARDWARE
The only needed part to be able to talk with the air conditioner is a 500 baud / 15V half-duplex UART signal with parity control. Using the specific hardware described below will make the library more reliable, robust and easy to use, including the following features:
- 15V to 3V3 level shifter
- Bus write detection / inhibit to avoid collisions
- Power supply consumption measurement
- External temperature sensor [TODO]
- External humidity sensor [TODO]
- External power switch [TODO]
- Easy to use connectors
- Error LED
- Mode selector to bypass the MCU and use the air conditioner as usual or put LCD in read-only mode

You can get details about how indoor unit uses the data bus in the (service schematics)[https://bulclima.com/uploads/fileattachment/files/originals/SM_ARHG%2030%2036%20LMLE.pdf] of the unit. The data bus is pull-up by 10k resistor to 15V and the data is writted/transmit using 390R pull down to GND by the controller using uPA2003 (darlington array). 
The data is readed using a comparator with treshold at 7.7v (bus is arround 15V) so any bellow 7.7V will be interpreted as logical 1. 

## DEBUG TOOLS
To get better understanding of the protocol and the library, there are some tools that can be used:
- Logic analyzer: to sniff the bus and see the packets. Set to 500 baud, 8N1, parity even.
- Oscilloscope: to see the signal and voltage levels.
- python script: included python script "reader.py". Connect ESP32 UART to the computer and write the assigned port in the py file (default is COM3). Run the script to decode the packets and see the parameters.

# TODO software
- [ ] BUG: At boot, LCD needs to send LOGIN message type (2) to UNIT to be able to control it [src: 32, dst: 1, onOff: 0, type:2, temp: 0, acmod: 0, fanm: 0, write: 0, login: 0, unk: 1, cp: 0, mgc: 0, error: 0]. UNIT reply [src: 0, dst: 32, onOff: 1, type:2, temp: 31, acmod: 7, fanm: 1, write: 0, login: 1, unk: 1, cp: 1, mgc: 0, error: 0]. If not, LCD shows error.
- [ ] if need to discard frame, (corrupt frame, incomplete...) not send anything and wait until new sincronization (100ms high) on UART line
- [ ] Default state is off. Get status from AC UNIT instead to avoid turning it off when MCU restarts
- [ ] restore state from eeprom if RESTORE_STATE_ON_STARTUP is defined
- [ ] Web interface to control AC
- [ ] Web API to control AC
- [ ] MQTT to control AC
- [ ] if cache changes and new value is not stored in cache then store it and also process, but if it is already in cache... Maybe is not processing it again?
- [ ] enable / disable LCD controller remote temperature sensor. If enabled, then send temperature from LCD remote sensor as sensor temperature, if not, temperature needs to be updated by API. 
- [ ] separate master and slave into RTOS tasks

# TODO HASSIO
- [ ] ESP32 thermostat with ESPhome and hassio: https://esphome.io/components/climate/thermostat.html HASSIO
- [ ] Home assistant integration, outside and inside temperature sensors connected to HASSIO
- [ ] Integrate heating system connected to HASSIO
- [ ] External temperature sensor connected to HASSIO
- [ ] External humidity sensor connected to HASSIO
- [ ] External UNIT power switch connected to HASSIO
- [ ] External EXTERIOR power switch connected to HASSIO
- [ ] Control humidifier: with external sensor. Connect humidifier to add humidity to A/C air input if house humidity decreases. Connected to HASSIO

# TODO hardware
- [ ] Hardware: Avoid pulldown data lines if reset...
- [ ] Test different pull-up resistor values (10k is used in indoor unit)
- [ ] create altium designer project
- [ ] UNIT Power consumption: with external sensor (current transformer)
- [ ] EXTERIOR Power consumption: with external sensor (current transformer)
- [ ] Fan only mode WITHOUT powering on external unit due to his high IDLE power consumption (1A). Use external or existing relay
- [ ] Bypass rele controlled by MCU. If MCU board is off, then bypass is on.

# Other AC models and information 
There are some information about AC control of different brands that maybe aren't compatible with this bus but can be an interesting resource to read:

- (Reverse engineer Fujitsu AC)[https://hackaday.io/project/19473-reverse-engineering-a-fujitsu-air-conditioner-unit]
- (Integrating Daikin)[https://community.openhab.org/t/integrating-a-daikin-a-c-system/31888]
- (mitsubishi2mqtt)[https://github.com/gysmo38/mitsubishi2MQTT/blob/master/src/mitsubishi2mqtt/mitsubishi2mqtt.ino]
- (M-NET-Sniffer)[https://github.com/LenShustek/M-NET-Sniffer]
- (MHI-AC-Ctrl)[https://github.com/absalom-muc/MHI-AC-Ctrl/tree/master]
- "Advanced" remote controller: Fujitsu 3NGF9024. Check manual to get more information about the protocol. [Manual UM_DMS002304492_20161007190704_EN](https://www.eurofred.com/es/accesorios-clima/accesorio-fujitsu-mando-a-distancia-con-cable-lcd-uty-rvnym/p/3NGF9024)
- IntesisBox: Gateway to modbus. [Manual](https://cdn.chipkin.com/assets/uploads/2018/feb/09-02-08-05_intesisbox_fj-rc-mbs-1_user_manual_es.pdf)
- IntesisBox: Gateway to KNX. [Manual](https://www.intesis.com/docs/librariesprovider11/manuals-design-guides/user-manuals/ac-interfaces/user-manual-inknxfgl001r000.pdf?sfvrsn=153750d7_18)
- Intesis Fujitsu gateways: (link)[https://www.intesis.com/es/productos/ac-interfaces/pasarelas-de-aire-acondicionado-fujitsu]
- https://github.com/geoffdavis/esphome-mitsubishiheatpump
- https://github.com/SwiCago/HeatPump
- https://nicegear.nz/blog/hacking-a-mitsubishi-heat-pump-air-conditioner/
- https://rjdekker.github.io/MHI2MQTT/
- https://community.openhab.org/t/mitsubishi-heavy-x-y-line-protocol/82898
- https://github.com/iblue/mhi-xy-bus/blob/main/report.pdf
- https://www.loxone.com/enen/kb/ac-control-air-for-mitsubishi-heavy-industries/
- https://github.com/crankyoldgit/IRremoteESP8266/issues/660
- https://github.com/Arnold-n/P1P2MQTT/blob/main/README.md
- https://github.com/Arnold-n/P1P2MQTT/tree/main
- add device to mqtt https://github.com/Arnold-n/P1P2MQTT/blob/main/doc/HomeAssistant.md