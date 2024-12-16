#pragma once
#include <cstdint>
#include <stdlib.h>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <linux/gpio.h>

/*
this is basically just catered to our use case 
impl. is ripped from jetgpio

*/
//note:
//spi buffer has to be huge 
//sudo rmmod spidev && sudo modprobe spidev bufsize=20480
// cat /sys/module/spidev/parameters/bufsiz
//can create /etc/modprobe.d/spidev.conf
//and add the line: 
//options spidev bufsiz=20480
#define SPI_BUFFER_SIZE 20480


#define SPI_MODE 0
#define SPI_CS_DELAY 0
#define SPI_CS_CHANGE 1
#define SPI_USE_LSB_FIRST 1 
#define SPI_BITS_WORD 8
#define SPI_DEV "/dev/spidev0.0"

#define SPI_MAX_SPEED 50000000 //50mbit/s

enum spi_state : int {
    SPI_DEFAULT = 0,
    SPI_CLOSED,
    SPI_OPEN,
    SPI_FAILED,
    SPI_MAX
};



struct spi_t{
    int32_t fd;
    uint32_t speed;
    spi_state state;
    
    spi_t(uint32_t speed) : fd(-1), speed(speed), state(SPI_CLOSED) {
        auto spi_error = [this](const char* error_msg){
            printf("[SPI] Error: %s \n", error_msg);
            this->state = SPI_FAILED;
        };
        if(speed > SPI_MAX_SPEED){
            spi_error("speed too high for the poor orin (max = 50mbits/s)"); return;
        }
        fd = open(SPI_DEV, O_RDWR);
        if(fd < 0){
            spi_error("failed to open spi device "); return;
        }
        uint32_t mode = SPI_MODE;
        uint32_t cs_delay = SPI_CS_DELAY;
        uint32_t cs_change = SPI_CS_CHANGE;
        uint32_t bits_word = SPI_BITS_WORD;
        uint32_t lsb_first = SPI_USE_LSB_FIRST;
        int err = 0;
        #define CHECK_IOCTL_ERROR(msg) if(err < 0) { spi_error(msg); return; } err = 0;

        err = ioctl(fd, SPI_IOC_WR_MODE, &mode);
        CHECK_IOCTL_ERROR("SPI MODE");
        err = ioctl(fd, SPI_IOC_RD_MODE, &mode);
        CHECK_IOCTL_ERROR("READ MODE");

        err = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits_word);
        CHECK_IOCTL_ERROR("BITS WORD");
        err = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits_word);
        CHECK_IOCTL_ERROR("READ BITS WORD");

        err = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
        CHECK_IOCTL_ERROR("SPEED");
        err = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
        CHECK_IOCTL_ERROR("READ SPEED");

        err = ioctl(fd, SPI_IOC_WR_LSB_FIRST, &lsb_first);
        CHECK_IOCTL_ERROR("LSB FIRST");
        err = ioctl(fd, SPI_IOC_RD_LSB_FIRST, &lsb_first);
        CHECK_IOCTL_ERROR("READ LSB FIRST");

        state = SPI_OPEN;
        printf("[SPI] Opened '%s' @ %.3f Mbits/s \n", SPI_DEV, (float)speed / (float)1000000.f);
    }
    bool transfer(char* tx_buffer, uint32_t len, char* rx_buffer = nullptr){
        spi_ioc_transfer tr = {
            .tx_buf = (uintptr_t)tx_buffer,
            .rx_buf = (uintptr_t)rx_buffer,
            .len = len,
            .speed_hz = speed,
            .delay_usecs = SPI_CS_DELAY,
            .bits_per_word = (uint8_t)SPI_BITS_WORD,
            .cs_change = (uint8_t)SPI_CS_CHANGE,
            .tx_nbits = 1u,
            .rx_nbits = 1u,
            .word_delay_usecs = 0u,
            .pad = 0u
        };
        int err = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
        if(err < 1){
            printf("[SPI] transfer error [%i] - strerr='%s' ", err, std::strerror(err));
            return false;
        }
        return true;
    }
    ~spi_t(){
        if(state != SPI_OPEN) return;
        close(fd);
        puts("[SPI] Closed SPI device");
        state = SPI_CLOSED; //lol no pt 
    }
};
