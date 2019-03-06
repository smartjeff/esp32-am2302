# esp32-am2302
A small library for reading data from the Aosong AM2302 humidity and temperature sensor.

## Including this library in your project
If not yet existant, create a `components` folder in your project root directory:
```
mkdir components
```

Then either

* add this project as a submodule:
```
git submodule add https://github.com/smartjeff/esp32-am2302.git components/am2302
```

* or clone it:
```
git clone https://github.com/smartjeff/esp32-am2302.git components/am2302
```

## Usage Example
**app_main.c**
```C
#include <stdio.h>
#include "freertos/FreeRTOS.h" // required for including task.h
#include "freertos/task.h"     // for vTaskDelay()
#include "am2302.h"

#define AM2302_DATA_PIN GPIO_NUM_23 // if your sensors data pin is connected to GPIO 23

void app_main() {
    if(am2302_init_bus(AM2302_DATA_PIN) != ESP_OK)
        printf("failed to initialize the bus!\n");
    while(1) {
        am2302_data_t data = am2302_read_data(AM2302_DATA_PIN);
        if(data.error != ESP_OK)
            printf("reading data from am2302 returned an error code!\n");
        printf(
            "%02.01f %% RH | %02.01f °C | parity: 0x%02hhx\n",
            (float) data.humidity / 10,
            (float) data.temperature / 10,
            data.parity
        );
        vTaskDelay(1000 / portTICK_PERIOD_MS); // read the data once per second
    }
}
```