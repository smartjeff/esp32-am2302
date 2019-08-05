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

static const char *TAG = "am2302";


/*
 * counts the microseconds that pass until the level on a pin changes
 * pin: the pin to observe
 * usec_max: the number of microseconds allowed to pass before aborting
 * level: the current level on the pin
 * returns: the number of microseconds that passed before the level changed
 * returns: -1 if usec_max was reached before the level changed
 */
static int await_level_change_usec(gpio_num_t pin, int usec_max, int level) {
    for(int i = 0; true; i++) {
        if(gpio_get_level(pin) != level)
            return i;
        if(i >= usec_max)
            return -1;
        ets_delay_us(1);
    }
}


/*
 * counts the microseconds that pass until the level on a pin changes
 * pin: the pin to observe
 * usec_min: the number of microseconds that have to pass before level change
 * usec_max: the number of microseconds allowed to pass before aborting
 * level: the current level on the pin
 * returns: true if the level changed after usec_min and before usec_max
 * returns: false otherwise
 */
static bool await_level_change(gpio_num_t pin, int usec_min, int usec_max, int level) {
    return await_level_change_usec(pin, usec_max, level) >= usec_min;
}


/*
 * reads incoming bits from a pin
 * pin: the pin to read from
 * n: the number of bits to read
 * *data: pointer to a variable for storing the bits
 * returns: false if the transmission failed
 * returns: true if the bits were read successfully
 */
static bool read_bits(gpio_num_t pin, int n, short int *data) {
    for(int i = 0; i < n; i++) {
        // wait for the sensor to pull the level up
        if(!await_level_change(pin, 0, 60, 0))
            return false;

        // measure the time the level is up
        int usecs = await_level_change_usec(pin, 80, 1);

        /*
         * if the function returned a value less than 0
         * the sensor pulled the level up for more than 80 microseconds
         */
        if(usecs < 0)
            return false;

        /*
         * if the level was pulled up longer than 50 microseconds: store 1
         * everything less is interpreted as 0 and not stored
         */
        if(usecs >= 50)
            *data |= (1U << (n - i - 1));
    }
    return true;
}


/*
 * reads the sensor data
 * pin: the pin the data pin of the sensor is connected to
 * returns: struct am2302_data_t {
 *      temperature:    the temperature as short int (divide it by 10)
 *      humidity:       the humidity as short int (divide it by 10)
 *      parity:         the parity data from the sensor used to verify temperature and humidity
 *      error:          ESP_OK if the data was received successfully and the parity check succeeded
 *                      an error if not
 * }
 */
am2302_data_t am2302_read_data(gpio_num_t pin) {
    am2302_data_t rv = {
        .humidity = 0,
        .temperature = 0,
        .parity = 0
    };

    // set pin direction to output to send the start signal
    if((rv.error = gpio_set_direction(pin, GPIO_MODE_OUTPUT)) != ESP_OK) {
        ESP_LOGE(TAG, "failed to enable output mode on pin (before start signal)");
        return rv;
    }

    // start signal: pull level down for 3ms to signal the sensor to start the transmission
    if((rv.error = gpio_set_level(pin, 0)) != ESP_OK) {
        ESP_LOGE(TAG, "failed to pull level down (transmission initiation)");
        return rv;
    }
    ets_delay_us(3000);

    // set pin direction to input to read the level
    if((rv.error = gpio_set_direction(pin, GPIO_MODE_INPUT)) != ESP_OK) {
        ESP_LOGE(TAG, "failed to enable input mode on pin (after start signal)");
        return rv;
    }

    rv.error = ESP_ERR_INVALID_RESPONSE;

    // after the start signal is sent the sensor will pull the level down, then up and then down again
    // wait for the sensor to pull the level down
    if(!await_level_change(pin, 0, 100, 1)) {
        ESP_LOGE(TAG, "level on pin is 0 after enabling input mode (transmission start signal)");
        ESP_LOGI(TAG, "the sensor has an internal pull up resistor for the data pin, so the sensor probably isn't connected");
        return rv;
    }

    // wait for the sensor to pull the level up
    if(!await_level_change(pin, 50, 120, 0)) {
        ESP_LOGE(TAG, "timed out waiting for level change or level change too quick (transmission start signal)");
        ESP_LOGI(TAG, "is the power source of the am2302 active?");
        return rv;
    }

    // wait for the sensor to pull the level down
    if(!await_level_change(pin, 50, 120, 1)) {
        ESP_LOGE(TAG, "timed out waiting for level change or level change too quick (transmission start signal)");
        return rv;
    }

    // read the humidity bits
    if(!read_bits(pin, 16, &rv.humidity)) {
        ESP_LOGE(TAG, "error reading humidity bits");
        return rv;
    }

    // read the temperature bits
    if(!read_bits(pin, 16, &rv.temperature)) {
        ESP_LOGE(TAG, "error reading temperature bits");
        return rv;
    }

    // read the parity bits
    if(!read_bits(pin, 8, (short int*) &rv.parity)) {
        ESP_LOGE(TAG, "error reading parity bits");
        return rv;
    }

    rv.error = ESP_OK;

    /*
     * parity check:
     * sum all of the 4 received temperature and humidity bytes
     * and check if it equals the parity byte
     */
    if((((rv.humidity >> 8) + rv.humidity + (rv.temperature >> 8) + rv.temperature) & 0xFF) != rv.parity) {
        rv.error = ESP_ERR_INVALID_RESPONSE;
        ESP_LOGW(TAG, "parity check failed");
    }

    /*
     * if the most significant temperature bit is 1, the temperature is negative.
     * since the am2302 uses a different system for representing negative numbers
     * we have to invert every bit and add 1.
     * afterwards we have to set the msb again (inverting unsets it)
     */
    if(rv.temperature & 0x8000)
        rv.temperature = (~rv.temperature + 1) | 0x8000;

    return rv;
}
