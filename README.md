# ControlZinvoltP1
Control the P1 dongle for the Zinvol VT1000 with an ESP32

The https://github.com/Leotro-Engineering/Universal-P1-Port-Dongle is used for the hardware. 

It alows you to modify a P1 DSRM smartmeter out so the battery can be controlled in more sophisticated way.

Key functions:
* relay, modified smart meter P1 telegrams
* Act as a simple P1 port reader via TCP. Real smart meter data
* Services a website with:
    * Actual smart meter data
    * Modified smart meter data (limited to actual power consumption/generation, total and per phase)
    * Show/set current mode:
        * Unmodified forward
        * Prevent charging/discharging (Off mode)
        * Force charging. (max, future given Watt)
        * Force discharing. (max, future given Watt)    
        * Charge only (future as battery info is needed)
        * Discharge only (future as battery info is needed)
    * Select on what phase the battery is connected
    * Select on what phase to modify the power consumption
* UI for connecting to the user Wlan
* REST API (GET) functions to:
    * Change mode 
    * Current battery status. SOC [%], current charge/discharge energy [Watt]
* Get actual data from the ESS API (future) and/or additional energy meter for enhanced control

* note: The P1 output port only gives output after an P1 message is received on the input. So it will not send more messages than it receives. It seems the Zinvolt controller doesn't work really well when the smart meters gives an output every 10 second and the data is send every second.
