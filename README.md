# smarthome-ESP

### Code flashing connections with FTDI board: 


| ESP12F        | FTDI          | 
| ------------- |:-------------:| 
| GND           | GND           |
| VCC           | 3.3V          |
| GPIO15        | GND           |
| GPIO0         | GND           |
| RESET         | HIGH          |

1. Open Arduino IDE, Open preferences window from  Arduino IDE. Go to File -> Preferences.
2. Enter the URL “http://arduino.esp8266.com/stable/package_esp8266com_index.json” into Additional Board Manager URLs field and click the “OK” button
3. Open Boards Manager. Go to Tools -> Board -> Boards Manager
4. Search ESP8266 board menu and install “esp8266 platform”
5. Choose your ESP8266 board from Tools > Board > Generic ESP8266 Module
6. Selected Board details with Port selection
8. Click on Upload Icon

### Wiring Sample: 

<img src="https://github.com/danishjoseph/smarthome-ESP/blob/main/wiring.jpg" alt="example for 2 relay module and 2 switch" height="500" width="500">
