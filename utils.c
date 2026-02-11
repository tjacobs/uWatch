// uWatch
//
// Utils
//

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"

// Forward declaration
void command(char* c);

// Shell commands
uint16_t comi=0;
char combufa[256]= {0};
int16_t comt;
uint8_t comc;
void shell() {
    comt = getchar_timeout_us(100);
    while (comt != PICO_ERROR_TIMEOUT) {
        comc = comt & 0xff;
        combufa[comi++] = comc;
        if (comc == '\n') {
            combufa[comi] = 0;
            combufa[comi - 1] = 0;
            command(&combufa[0]);
            comi = 0;
        }
        if (comi == 254) comi = 0;
        comt = getchar_timeout_us(100);
    }
}

// CRC
uint8_t crc(uint8_t *addr, uint32_t len) {
    uint8_t crc = 0;
    while (len != 0) {
        uint8_t i;
        uint8_t in_byte = *addr++;
        for (i = 8; i != 0; i--) {
            uint8_t carry = (crc ^ in_byte ) & 0x80;
            crc <<= 1;
            if (carry != 0) {
                crc ^= 0x7;
            }
            in_byte <<= 1;
        }
        len--;
    }
    return crc;
}

// I2C
#define AHT15 0x38
#define I2C0 i2c0
#define I2C_SDA 4
#define I2C_SCL 5

void i2c_scan() {
    // Scan
    i2c_init(I2C0, 100 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    bi_decl(bi_2pins_with_func(I2C_SDA, I2C_SCL, GPIO_FUNC_I2C));
    printf("\nI2C Bus Scan\n");
    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");
    for (int addr = 0; addr < (1 << 7); ++addr) {
        if (addr % 16 == 0) printf("%02x ", addr);
        int ret;
        uint8_t rxdata=0;

        // Check reserved addresses, addresses of the form 000 0xxx or 111 1xxx are reserved
        bool is_reserved = (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
        if (is_reserved) ret = PICO_ERROR_GENERIC;
        else ret = i2c_read_blocking(I2C0, addr, &rxdata, 1, false);
        printf(ret < 0 ? "." : "@");
        printf(addr % 16 == 15 ? "\n" : "  ");
    }
}

void i2c_read() {
    // Read
    uint8_t cmd = 0;
    uint8_t data[6];

    // Reset
    cmd = 0xE1;
    i2c_write_blocking(I2C0, AHT15, &cmd,1, true);
    sleep_ms(100);

    // Read
    cmd = 0xAC;
    i2c_write_blocking(I2C0, AHT15, &cmd,1, true);
    sleep_ms(100);
    i2c_read_blocking( I2C0, AHT15, &data[0],6, false);
    printf("0x%02x 0x%02x\n",data[0],data[1]);
    printf("0x%02x 0x%02x\n",data[2],data[3]);
    printf("0x%02x 0x%02x\n",data[4],data[5]);

    // 20-bit raw humidity data
    //uint32_t humidity   = data[1]; 
    //         humidity <<= 8;
    //         humidity  |= data[2];
    //         humidity <<= 4;
    //         humidity  |= data[3] >> 4;
    //if (humidity > 0x100000) {humidity = 0x100000;}
    //float hum = ((float)humidity / 0x100000) * 100;

    // 20-bit raw temperature data
    //uint32_t temperature   = data[3] & 0x0F;                
    //         temperature <<= 8;
    //         temperature  |= data[4];
    //         temperature <<= 8;
    //         temperature  |= data[5];

    //float tem = ((float)temperature / 0x100000) * 200 - 50;
    //printf("%f %f\n",hum,tem);
}
