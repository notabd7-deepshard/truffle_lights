#!/bin/bash

echo "setting up SPI, this will reboot the orin"

sudo cp jetson-io-hdr40-user-custom.dtbo /boot/jetson-io-hdr40-user-custom.dtbo

sudo reboot now
