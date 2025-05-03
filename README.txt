very simple WS2812B Driver code for the ORIN AGX

usage:

as root run ./enable_spi.sh, this will hopefully enable SPI and reboot the orin 

then compile with 'make clean all'
(for the above command youll have to mnaully add state youd like to test)
then run as root, ./ledtest 


to test all states together run 'make state_test'
then run as root, ./led_state_test 
it will cycle through all states 


LED PINS:
Pinout can be found here: https://jetsonhacks.com/nvidia-jetson-agx-orin-gpio-header-pinout/

Pin 2: 5VDC  -> WS2812 Vin 
Pin 6: GND -> WS2812 GND
Pin 19: SPI1 MOSI -> WS2812 DataIn


Set the LED_COUNT  macro at the top of ledcontrol.h to the number of LEDs connected to the Orin.
