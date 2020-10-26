# REFLEXArduinoCode
 REFLEX is a camera startup, with the aim of creating a modular camera. 
 
 This set of code was used to program the Arduino (BLE Nano) controller. The Arduino BLE Nano was used to run the camera's feature using either BLE (via UART*) or UART to send commands.
 
\* The Arduino BLE Nano has a mode where it transmits all serial (UART)
 data via BLE.
 
 Features include:

- A physical rotary dial, that can change values based on the mode.
- A physical push button, that triggers the shutters to close at shutter_speed. (Release first curtain to open, delay for shutter_speed milliseconds to expose the film, release second curtain to close)
- A physical push button, that iniates a light sensor reading and the related calculations that go with that.
- Software defined modes: ISO priority, Shutter priority, Aperture priority.
- Configure TSL2561 light sensor in order to get the correct readings in lux.
- Calculate exposure settings based on the current mode.
