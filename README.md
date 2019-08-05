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

And if you're setting the `COMPONENTS` variable in your `CMakeLists.txt`, add it there:
```CMake
set(COMPONENTS main esptool_py am2302)
```

## Usage Example
**app_main.c**
```C
#include <stdio.h>
#include "freertos/FreeRTOS.h"      // required for including task.h
#include "freertos/task.h"          // for vTaskDelay()
#include "am2302.h"

#define AM2302_DATA_PIN GPIO_NUM_23 // if your sensors data pin is connected to GPIO 23

void app_main() {
    while(1) {
        // wait one second between the readings
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        // read the sensor data
        am2302_data_t data = am2302_read_data(AM2302_DATA_PIN);

        // check if the data has been receieved successfully
        if(data.error != ESP_OK)
            printf("reading data from am2302 returned an error!\n");

        // print the data
        printf(
            "%02.01f %% RH | %02.01f °C | parity: 0x%02hhx\n",
            (float) data.humidity / 10,
            (float) data.temperature / 10,
            data.parity
        );
    }
}
```

## License
This project is licensed under the GNU General Public License v3.0, see [LICENSE](LICENSE).

## Special Thanks
We used [esp32-websocket](https://github.com/Molorius/esp32-websocket) as a template for our am2302 component.
Thanks to Blake Felt for creating it!
