# Programmable-Voltage-Reference
An adjustable 0.001 to 5.000 volt reference, adjustable in 1 mV steps with display
==============================

<img src="https://github.com/Barbouri/Programmable-Voltage-Reference/blob/a182458ea60cbced789c04f60beefa2226b19169/PVRFrontPanel2500-1200.JPG" alt="Programmable Voltage Reference with 1 mV resolution and 50 uV accuracy" height="525" width="1000">
This repository contains the design files and write-up for a Programmable Voltage Reference.  The reference has a range of 1 mV to 5.000 volts in 1 mV steps.  The accuracy is plus or minus 50 uV for any set value.  The reference value is displayed on a 2 X 16 charactor OLED display and can be set manually using the rotary encoder or remotely over the TTL serial to isolated USB 2.0 port.

The software folder contains an Arduino / Teensyduino sketch that is the source code for the voltage reference firmware.

Visit HTTPS://www.barbouri.com for a more detailed explanation of the project design.

You can order this PCB directly from OSH Park.  Click on the following link.  
  * Programmable Voltage Reference - https://oshpark.com/shared_projects/aFUNW4O5 

<img src="https://raw.githubusercontent.com/uChip/VoltageReferenceProgrammable/master/RevDtop.png" alt="PCB Top" height="287" width="550">

<img src="https://raw.githubusercontent.com/uChip/VoltageReferenceProgrammable/master/RevDbottom.png" alt="PCB Bottom" height="287" width="550">

See the Bill of Materials (BOM) file in the repo Hardware folder for a parts list.  

## Status  
  * Version 3.14 PCB has been tested to be functional.  

## File Formats  

Hardware design files are in "CadSoft EAGLE PCB Design Software 7.7" .brd and .sch formats.  A free version of the software can be downloaded from www.cadsoftusa.com. 

The example code is in Arduino .ino format (text).  A free version of the Arduino software can be downloaded from www.arduino.cc.  

## Distribution License  

License:
<a rel="license" href="http://creativecommons.org/licenses/by-sa/4.0/"><img alt="Creative Commons License" style="border-width:0" src="https://i.creativecommons.org/l/by-sa/4.0/88x31.png" /></a><br /><span xmlns:dct="http://purl.org/dc/terms/" property="dct:title">Programmable Voltage Reference</span> by <a xmlns:cc="http://creativecommons.org/ns#" href="https://github.com/uChip/VoltageReferenceProgrammable" property="cc:attributionName" rel="cc:attributionURL">C.Schnarel</a> is licensed under a <a rel="license" href="http://creativecommons.org/licenses/by-sa/4.0/">Creative Commons Attribution-ShareAlike 4.0 International License</a>.
  
