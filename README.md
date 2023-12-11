Belgian smart meter reader and load switching system.

Reads the smart meter P1-port and switches consumers on and off based on total power consumption and injection.
Belgian smart meter communication is based on the Dutch DSMR 5.0 protocol, but there are (undocumented) differences.
Tested for the Sagemcom Siconia T211 meter. 

Hardware:
- Arduino Mega 2560
- IC 7404 NOT gate inverter
- TRU COMPONENTS TC-9927216 relay module, 4x 10A 250V relays (for switching the load-switching relays)
- 25 Ampère 250 Volt relays (for load-switching)

P1-port communication:
- serial port
- 5 Volt
- 115200bps
- 8N1: 8 bits, 1 stopbit
- inverted logic

P1-port pinout:
pin 1 +5V (for peripheral device)
pin 2 Data_Request: Arduino pin 8
pin 3 Data_GND: Arduino GND
pin 4 NC
pin 5 Data (open collector): IC 7404 pin 1, via 4.7kOhm to Arduino +5V
pin 6 Power_GND (for peripheral device)

IC 7404 pinout:
pin 1: P1 pin 5 (Data)
pin 2: Arduino 19RX1
pin 7: Arduino GND
pin 14: Arduino +5V

Arduino Mega 2560 pinout:
outputs:
pin +5V: IC 7404 pin 14, P1 pin 2, relay module VCC
pin GND: IC 7404 pin 7, P1 pin 3, pushbutton, relay module GND
pin 2: relay 1
pin 3: relay 2
pin 4: relay 3
pin 5: relay 4
pin 8: P1 pin 2 (Data_Request)
pin 13: internal LED
inputs:
pin 19RX1: IC 7404 pin 2
pin 9: pushbutton


Created 04 12 2023
By Wouter van Geldre
Modified under development
By Wouter van Geldre
