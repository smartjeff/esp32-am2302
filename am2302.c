/*
 * esp32-am2302 - A small library for reading data from the Aosong AM2302 humidity and temperature sensor.
 * Copyright (C) 2019  hjkgfdgf jkhsjdhjs
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "am2302.h"
#include "esp_log.h"

#define GPIO_LOW 0
#define GPIO_HIGH 1

static const char *TAG = "am2302";

static int await_level_change_usec(gpio_num_t pin, int usec_max, int level) {
    for(int i = 0; true; i++) {
        if(gpio_get_level(pin) != level)
            return i;
        if(i >= usec_max)
            return -1;
        ets_delay_us(1);
    }
}

static inline bool await_level_change(gpio_num_t pin, int usec_min, int usec_max, int level) {
    return await_level_change_usec(pin, usec_max, level) >= usec_min;
}

static bool read_bits(gpio_num_t pin, int n, short int *data) {
    for(int i = 0; i < n; i++) {
        if(!await_level_change(pin, 0, 60, GPIO_LOW))
            return false;
        int usecs = await_level_change_usec(pin, 80, GPIO_HIGH);
        if(usecs < 0)
            return false;
        if(usecs >= 50)
            *data |= (1U << (n - i - 1));
    }
    return true;
}

esp_err_t am2302_init_bus(gpio_num_t pin) {
    esp_err_t rv;
    if((rv = gpio_set_direction(pin, GPIO_MODE_OUTPUT)) != ESP_OK) {
        ESP_LOGE(TAG, "failed to enable output mode on pin");
        return rv;
    }
    if((rv = gpio_set_level(pin, GPIO_HIGH)) != ESP_OK) {
        ESP_LOGE(TAG, "failed to pull level up");
        return rv;
    }
    ets_delay_us(1000);
    return ESP_OK;
}

am2302_data_t am2302_read_data(gpio_num_t pin) {
    am2302_data_t rv = {
            .temperature = 0,
            .humidity = 0,
            .parity = 0
    };
    if((rv.error = gpio_set_direction(pin, GPIO_MODE_OUTPUT)) != ESP_OK) {
        ESP_LOGE(TAG, "failed to enable output mode on pin");
        return rv;
    }
    if((rv.error = gpio_set_level(pin, GPIO_LOW)) != ESP_OK) {
        ESP_LOGE(TAG, "failed to pull level down");
        return rv;
    }
    ets_delay_us(3000);
    if((rv.error = gpio_set_level(pin, GPIO_HIGH)) != ESP_OK) {
        ESP_LOGE(TAG, "failed to pull level up");
        return rv;
    }
    if((rv.error = gpio_set_direction(pin, GPIO_MODE_INPUT)) != ESP_OK) {
        ESP_LOGE(TAG, "failed to enable input mode on pin");
        return rv;
    }
    rv.error = ESP_ERR_INVALID_RESPONSE;
    if(!await_level_change(pin, 0, 500, GPIO_HIGH)) {
        ESP_LOGE(TAG, "timed out waiting for level change");
        return rv;
    }
    if(!await_level_change(pin, 50, 120, GPIO_LOW)) {
        ESP_LOGE(TAG, "timed out waiting for level change or level change too quick");
        ESP_LOGI(TAG, "is the power source of the am2302 active?");
        return rv;
    }
    if(!await_level_change(pin, 50, 120, GPIO_HIGH)) {
        ESP_LOGE(TAG, "timed out waiting for level change or level change too quick");
        return rv;
    }
    if(!read_bits(pin, 16, &rv.humidity)) {
        ESP_LOGE(TAG, "error reading humidity bits");
        return rv;
    }
    if(!read_bits(pin, 16, &rv.temperature)) {
        ESP_LOGE(TAG, "error reading temperature bits");
        return rv;
    }
    if(!read_bits(pin, 8, (short int*) &rv.parity)) {
        ESP_LOGE(TAG, "error reading parity bits");
        return rv;
    }
    if((((rv.humidity >> 8) + rv.humidity + (rv.temperature >> 8) + rv.temperature) & 0xFF) != rv.parity) {
        ESP_LOGW(TAG, "parity check failed");
        return rv;
    }
    if(rv.temperature & 0x8000)
        rv.temperature = (~rv.temperature + 1) | 0x8000;
    rv.error = ESP_OK;
    return rv;
}
