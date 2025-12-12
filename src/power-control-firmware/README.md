# MEGAphone Power Control firmware

For low-power operation the MEGAphone uses an IceSugar Nano Lattice-based FPGA to
monitor the cellular modem for important events (RING for incoming calls, and +QIND
for incoming SMS and other configurable events).

The firmware intentionally does very little, so as to use as little power as possible.

Essentially it relays the UART from the cellular modem to the main FPGA (if it is turned on),
and allows the main FPGA to request power to be turned on and off as required.

It does one extra thing that allows the main FPGA to be turned off most of the time:
Monitoring the cellular modem for those events mentioned above.

The end results fits in a ~1K LC FPGA.
