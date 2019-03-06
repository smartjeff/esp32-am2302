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

#ifndef AM2302_H
#define AM2302_H

#include "esp_err.h"
#include "driver/gpio.h"


typedef struct {
    short int humidity;
    short int temperature;
    unsigned char parity;
    esp_err_t error;
} am2302_data_t;

esp_err_t am2302_init_bus(gpio_num_t pin);
am2302_data_t am2302_read_data(gpio_num_t pin);

#endif //AM2302_H
