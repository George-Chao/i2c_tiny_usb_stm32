/*
 * usb_i2c.c
 *
 *  Created on: 2020-10-03 08:50
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include "stm32f1xx_hal.h"

#include "tusb.h"

#include "core/os.h"
#include "chip/i2c.h"
#include "user/usb_i2c.h"

#define TAG "usb_i2c"

static HAL_StatusTypeDef status = HAL_OK;
static uint8_t buff[CFG_TUD_ENDPOINT0_SIZE] = {0};

static uint32_t i2c_smbus_funcs = I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;

// Invoked when received VENDOR control request
bool tud_vendor_control_request_cb(uint8_t rhport, tusb_control_request_t const *request)
{
    switch (request->bRequest) {
    case CMD_GET_FUNC:
        memcpy(buff, &i2c_smbus_funcs, sizeof(i2c_smbus_funcs));

        return tud_control_xfer(rhport, request, (void *)buff, sizeof(i2c_smbus_funcs));
    case CMD_SET_DELAY:
        return tud_control_xfer(rhport, request, (void *)buff, 0);
    case CMD_I2C_IO:
    case CMD_I2C_IO | CMD_I2C_IO_BEGIN:
    case CMD_I2C_IO | CMD_I2C_IO_END:
    case CMD_I2C_IO | CMD_I2C_IO_BEGIN | CMD_I2C_IO_END: {
        uint8_t read = request->wValue & I2C_M_RD;
        uint8_t addr = request->wIndex;
        uint8_t size = request->wLength;
        uint32_t err = HAL_I2C_GetError(&i2c1);

        if (status != HAL_OK && err != (err & HAL_I2C_ERROR_AF)) {
            OS_LOGW(TAG, "hardware reset");

            i2c1_reset();
        }

        if (read) {
            status = HAL_I2C_Master_Receive(&i2c1, addr << 1, (uint8_t *)buff, size, 500);
        } else if (!size) {
            status = HAL_I2C_Master_Transmit(&i2c1, addr << 1, (uint8_t *)buff, size, 500);
        }

        return tud_control_xfer(rhport, request, (void *)buff, size);
    }
    case CMD_GET_STATUS:
        if (status != HAL_OK) {
            buff[0] = STATUS_ADDRESS_NACK;
        } else {
            buff[0] = STATUS_ADDRESS_ACK;
        }

        return tud_control_xfer(rhport, request, (void *)buff, 1);
    default:
        return false;
    }

    return true;
}

// Invoked when DATA Stage of VENDOR's request is complete
bool tud_vendor_control_complete_cb(uint8_t rhport, tusb_control_request_t const *request)
{
    (void)rhport;

    switch (request->bRequest) {
    case CMD_I2C_IO:
    case CMD_I2C_IO | CMD_I2C_IO_BEGIN:
    case CMD_I2C_IO | CMD_I2C_IO_END:
    case CMD_I2C_IO | CMD_I2C_IO_BEGIN | CMD_I2C_IO_END: {
        uint8_t read = request->wValue & I2C_M_RD;
        uint8_t addr = request->wIndex;
        uint8_t size = request->wLength;

        if (!read && size) {
            status = HAL_I2C_Master_Transmit(&i2c1, addr << 1, (uint8_t *)buff, size, 500);
        }

        break;
    }
    default:
        break;
    }

    return true;
}
