/**
 * @file SERVER_v1.3_Firebase_and_ThingSpeak.ino
 * @author Monika Rabka
 * @brief 
 * @version 0.1
 * @date 2021-04
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "SPIFFS.h"
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <iomanip>
#include <sstream>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <fstream>
#include <iostream>
#include <FirebaseESP32.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ThingSpeak.h>
#include <WiFiClient.h>

//Initialization of library objects
TinyGPSPlus gps;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

WiFiClient client;

//Variables to save date and time
String formattedDay;
String formattedTime;

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

//FirebaseESP32 data object and path
FirebaseData firebaseData;
FirebaseJson jsonFirebase;

//First node of the Firebase node tree
String board_name = "/Board 1/";
String path;

//GPS lat and long JSON to be sent with sensor data
FirebaseData firebaseDataGPS;
FirebaseJson jsonFirebaseGPS;
String lat_long;
bool received;

//Web server port number to 80 and /events event source
AsyncWebServer server(80);
AsyncEventSource events("/events");

//GPS serial ports definition
static const int RXD2 = 16, TXD2 = 17;
static const uint32_t GPSBaud = 9600;

//JSON objects for deserializing and serializing the received sensor values
String jsonToParse;
StaticJsonDocument<300> doc;
StaticJsonDocument<300> board;

double longitudeDouble;
String longitude;
double latitudeDouble;
String latitude;

double BME280temp;
double BME280hum;
double BME280pres;
double CCS811co2;
double CCS811tvoc;
double DS18B20;
double FC28;
double LDR;

int thingspeaktimer;
int thingspeakdelay = 1800000;

/**
// To be used if Static IP address, gateway, and subnet are needed 
IPAddress local_IP(192, 168, 1, 253);
IPAddress gateway(192, 168, 1, 254);
IPAddress subnet(255, 255, 0, 0);
*/

String converter(uint8_t *str)
{
  return String((char *)str);
}

// **********************************************************
// Deserialize the JSON document
void parseJson(String json)
{
  DeserializationError error = deserializeJson(doc, json);
  // Test if parsing was successful
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  //Deserialises the JSON received in the HTTP POST request body
  BME280temp = doc["airtemperature"];
  BME280hum = doc["airhumidity"];
  BME280pres = doc["airpressure"];
  CCS811co2 = doc["eCO2"];
  CCS811tvoc = doc["tVOC"];
  DS18B20 = doc["soiltemperature"];
  FC28 = doc["soilmoisture"];
  LDR = doc["LDR"];

  //Print the deserialised sensor values in Serial Monitor
  Serial.println();
  Serial.println(BME280temp);
  Serial.println(BME280hum);
  Serial.println(BME280pres);
  Serial.println(CCS811co2);
  Serial.println(CCS811tvoc);
  Serial.println(DS18B20);
  Serial.println(FC28);
  Serial.println(LDR);

  //Creates a new JSON object to fit with the html structure
  board["id"] = "1";
  board["airtemperature"] = BME280temp;
  board["airhumidity"] = BME280hum;
  board["airpressure"] = BME280pres;
  board["soiltemperature"] = DS18B20;
  board["soilmoisture"] = FC28;
  board["eCO2"] = CCS811co2;
  board["tVOC"] = CCS811tvoc;
  board["LDR"] = LDR;

  String jsonStr;
  serializeJson(board, jsonStr);
  events.send(jsonStr.c_str(), "new_readings", millis());
}

String formatString(double dbl)
{
  std::stringstream stream;
  stream << std::fixed << std::setprecision(8) << dbl;
  std::string s = stream.str();
  return s.c_str(); // string to String
}

//Function to get GPS functions to be displayed on the website
String processor(const String &var)
{
  if (var == "LONGITUDE")
  {
    return getLongitude();
  }
  else if (var == "LATITUDE")
  {
    return getLatitude();
  }
  return String();
}

static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do
  {
    while (Serial2.available())
      gps.encode(Serial2.read());
  } while (millis() - start < ms);
}

//Gets longitude and stores it to variables
String getLongitude()
{
  double double_longitude = (gps.location.lng());
  String longitude = formatString(double_longitude);
  //Serial.println(longitude);
  return longitude;
}

//Gets latitude and stores it in variables
String getLatitude()
{
  double double_latitude = (gps.location.lat());
  String latitude = formatString(double_latitude);
  // Serial.println(latitude);
  return latitude;
}

void setup()
{
  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); //GPS

  // Initialize SPIFFS
  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Connect to Wi-Fi network with SSID and password
  File root = SPIFFS.open("/");
  File file = root.openNextFile();

  while (file)
  {
    Serial.print("FILE: ");
    Serial.println(file.name());
    file = root.openNextFile();
  }

  /**
    // Configures static IP address
    if (!WiFi.config(local_IP, gateway, subnet)) {
     Serial.println("STA Failed to configure");
    }
  */

  WiFi.mode(WIFI_AP_STA);

  //access point part
  Serial.println("Creating Access Point");
  WiFi.softAP(assid, asecret);
  Serial.print("IP address:\t");
  Serial.println(WiFi.softAPIP());

  //station part
  Serial.print("connecting to...");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  ThingSpeak.begin(client); // Initialize ThingSpeak

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH); // Initialize Firebase
  Firebase.reconnectWiFi(true);

  //Set database read timeout to 1 minute (max 15 minutes)
  Firebase.setReadTimeout(firebaseData, 1000 * 60);
  //tiny, small, medium, large and unlimited.
  //Size and its write timeout e.g. tiny (1s), small (10s), medium (30s) and large (60s).
  Firebase.setwriteSizeLimit(firebaseData, "small");

  // Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(3600);

  thingspeaktimer = millis();

  // *******************************************************************************************************************************************************************
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html", String(), false, processor); });

  server.on(
      "/data", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
      {
        jsonToParse = converter(data);
        parseJson(jsonToParse);
        received = true;
        request->send(200, "text/plain", "Received the data!");
      });

  server.on("/longitude", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", getLongitude().c_str()); });
  server.on("/latitude", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/plain", getLatitude().c_str()); });

  events.onConnect([](AsyncEventSourceClient *client)
                   {
                     if (client->lastId())
                     {
                       Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
                     }
                     // send event with message "hello!", id current millis
                     // and set reconnect delay to 1 second
                     client->send("hello!", NULL, millis(), 10000);
                   });
  server.addHandler(&events);
  server.begin();
}

void loop()
{
  smartDelay(1000);

  //Check the GPS and if it has longitude and latitude then upload to the Firebase
  if (millis() > 5000 && gps.charsProcessed() < 10)
  {
    Serial.println(F("No GPS data received: check wiring"));
  }
  else
  {
    Serial.println("Get GPS coordinates");
    lat_long = getLatitude() + ", " + getLongitude();
    jsonFirebaseGPS.add("GPS", lat_long);
    //Set the GPS child node in the Firebase under the main node "/Board 1"
    if (Firebase.setJSON(firebaseDataGPS, board_name + "Location", jsonFirebaseGPS))
    {
      Serial.println(firebaseData.dataPath());
      Serial.println(firebaseData.pushName());
      Serial.println(firebaseData.dataPath() + "/" + firebaseData.pushName());
    }
    else
    {
      Serial.println(firebaseData.errorReason());
    }
  }

  //Update the time
  while (!timeClient.update())
  {
    timeClient.forceUpdate();
  }

  //Store the day and time values to use in the Firebase path later
  formattedDay = timeClient.getFormattedDate() + "/";
  formattedTime = timeClient.getFormattedTime();

  //If there are values received from the Client then add to Firebase (don't upload when variables store 0)
  if (received)
  {
    jsonFirebase.add("BME280_Temp", BME280temp);
    jsonFirebase.add("BME280_Hum", BME280hum);
    jsonFirebase.add("BME280_Pres", BME280pres);
    jsonFirebase.add("CCS811_CO2", CCS811co2);
    jsonFirebase.add("CCS811_tVOC", CCS811tvoc);
    jsonFirebase.add("Soil_Temp", DS18B20);
    jsonFirebase.add("Soil_Moisture", FC28);
    jsonFirebase.add("LDR", LDR);

    //Sets the JSON object in the firebase on a specific time/day node instead of auto generated paths
    if (Firebase.setJSON(firebaseData, board_name + formattedDay + formattedTime, jsonFirebase))
    {
      Serial.println(firebaseData.dataPath());
      Serial.println(firebaseData.pushName());
      Serial.println(firebaseData.dataPath() + "/" + firebaseData.pushName());
    }
    else
    {
      Serial.println(firebaseData.errorReason());
    }
  }
  // ****************************************************************************************
  // Sends the values to ThingSpeak every 30mins
  if ((millis() - thingspeaktimer) > thingspeakdelay)
  {
    sendToThingSpeak();
    thingspeaktimer = millis();
  }
  delay(900000);
}
//************************************************************************
//************************************************************************
// Sets ThingSpeak fields with the stored sensor values
void sendToThingSpeak()
{
  // set the fields with the values
  ThingSpeak.setField(1, float(BME280temp));
  ThingSpeak.setField(2, float(BME280hum));
  ThingSpeak.setField(3, float(BME280pres));
  ThingSpeak.setField(4, float(CCS811tvoc));
  ThingSpeak.setField(5, float(CCS811co2));
  ThingSpeak.setField(6, float(DS18B20));
  ThingSpeak.setField(7, float(FC28));
  ThingSpeak.setField(8, float(LDR));
  // write to the ThingSpeak channel
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if (x == 200)
  {
    Serial.println("Channel update successful.");
  }
  else
  {
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }
}
