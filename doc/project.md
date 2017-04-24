@defgroup curie_streaming curie_streaming
@{
@ingroup projects_list

###Hardware target

This project runs on ORB or CRB.

###Features

It includes:
- ISPP and IASP
- Sensor data streaming
- Fuel gauge
- Watchdog management

It supports log on USB and UART.

###Power Button Management

Power button press can trigger the following:
- short press (<1s) to start/stop raw sensor data streaming
- long press (>=1s) to shutdown the board
- very long press (>=6s) to clear bonding information

###LED Feedback

- 1s green at boot
- 1s white at sensor data streaming start
- 1s blue at sensor data streaming stop

###Behaviour

####Raw sensor data streaming
Raw sensor data are sent through IASP.
Raw sensor data streaming can be started (and ended) using either power button or BLE.
@}
