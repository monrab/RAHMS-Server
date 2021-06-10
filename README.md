# RAHMS-Server
This repo contains the .ino and .html files needed for the Real-time Autonomous Horticulture Monitoring System (RAHMS) Server to receive the JSON String with sensor values from the RAHMS Client and display them on its website, send them to ThingSpeak and Firebase Real-time Database using HTTP POST requests. 

## Circuit Diagram
![server diagram](https://github.com/monrab/assets/blob/main/Schematic_rahms%20server_2021-04-23.png?raw=true)

## Server website (debugging)
This website created on AsyncWebServer is optional - used for debugging purposes during testing stages.
The data folder which includes the website's HTML file needs to be added into the internal memory of the ESP32 microcontroller (SPIFFS) - this can be done by adding a plugin into Arduino IDE. Tutorial [here](https://randomnerdtutorials.com/install-esp32-filesystem-uploader-arduino-ide/). 

![server website](https://github.com/monrab/assets/blob/main/Screenshot_2021-05-08%20RAHMS%20Server%20-%20Copy.png?raw=true)

## Firebase node tree

![firebase-node-tree](https://github.com/monrab/assets/blob/main/Changes%20to%20Firebase%20-%20final%20tree%20structure%20-%20Copy.jpg?raw=true)

### Arduino
The credentials needed to be set are at the beggining of the sketch
```c++
//Network credentials
const char *ssid = "********";
const char *password = "********";

const char *assid = "ESP32-AP";
const char *asecret = "123456789";

//Firebase credentials
String FIREBASE_HOST = "https://xxx.firebaseio.com/";
String FIREBASE_AUTH = "*********************************";

//ThingSpeak credentials
unsigned long myChannelNumber = 123456789;
const char *myWriteAPIKey = "**************";
}
```
as well as the ThingSpeak and Firebase upload delays which sets how often the sensor values will be posted to the channel/database.
```c++
int thingspeak_timer;
int thingspeak_delay = 1800000;
int firebase_timer;
int firebase_delay = 900000;
```

