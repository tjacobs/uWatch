// uWatch
//
// Utils
//

#include "hardware/gpio.h"
#include "hardware/i2c.h"

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
        if (addr % 16 == 0) {
            printf("%02x ", addr);
        }
        int ret;
        uint8_t rxdata=0;
        if (reserved_addr(addr))
            ret = PICO_ERROR_GENERIC;
        else
            ret = i2c_read_blocking(I2C0, addr, &rxdata, 1, false);

        printf(ret < 0 ? "." : "@");
        printf(addr % 16 == 15 ? "\n" : "  ");
        //if(ret>=0 && addr==HIH71){has_hih=true;}
    }
}

void i2c_read() {
    uint8_t cmd=0;
    uint8_t data[6];

    //cmd = 0b11100001; // init
    cmd = 0xE1; // reset
    i2c_write_blocking(I2C0, AHT15, &cmd,1, true);
    sleep_ms(100);

    //cmd = 0b10101100; // read
    cmd = 0xAC;
    i2c_write_blocking(I2C0, AHT15, &cmd,1, true);
    sleep_ms(100);
    i2c_read_blocking( I2C0, AHT15, &data[0],6, false);
    printf("0x%02x 0x%02x\n",data[0],data[1]);
    printf("0x%02x 0x%02x\n",data[2],data[3]);
    printf("0x%02x 0x%02x\n",data[4],data[5]);

    //uint32_t humidity   = data[1];                          //20-bit raw humidity data
    //         humidity <<= 8;
    //         humidity  |= data[2];
    //         humidity <<= 4;
    //         humidity  |= data[3] >> 4;
    //if (humidity > 0x100000) {humidity = 0x100000;}             //check if RH>100, no need to check for RH<0 since "humidity" is "uint"
    //float hum = ((float)humidity / 0x100000) * 100;

    //uint32_t temperature   = data[3] & 0x0F;                //20-bit raw temperature data
    //         temperature <<= 8;
    //         temperature  |= data[4];
    //         temperature <<= 8;
    //         temperature  |= data[5];

    //float tem = ((float)temperature / 0x100000) * 200 - 50;
    //printf("%f %f\n",hum,tem);

}
