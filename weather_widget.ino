

#define DEBUG
#define DEBUG_NO_PAGE_FADE
#define DEBUG_NO_TOUCH

#define nexSerial Serial1

//
// include libraries
//

#include <Wire.h>
#include <SPI.h>                                // I2C library
#include <Adafruit_BME280.h>                    // BME280 library
#include <ArduinoJson.h>                        // https://github.com/bblanchon/ArduinoJson
#include <ESP8266WiFi.h>                        // WiFi library
#include <WiFiUdp.h>                            // Udp library
#include <TimeLib.h>                            // Time library

extern "C" {
  #include "user_interface.h"
}

//
// settings for WiFi connection
//

char * ssid     = "Ujjwal Kumar";         // WiFi SSID
char * password = "ujjwalk450";         // WiFi password

unsigned int localPort = 2390;                  // local port to listen for UDP packets
IPAddress timeServerIP;                         // IP address of random server 
const char* ntpServerName = "pool.ntp.org";     // server pool
byte packetBuffer[48];                          // buffer to hold incoming and outgoing packets
int timeZoneoffsetGMT = 19800;                   // offset from Greenwich Meridan Time
boolean DST = false;                            // daylight saving time
WiFiUDP clockUDP;                               // initialize a UDP instance

//
// settings for the openweathermap connection
// sign up, get your api key and insert it below
//

char * servername ="api.openweathermap.org";          // remote server with weather info
String APIKEY = "ee83b3bec7bcd0c2beccac0199d38c04";   // personal api key for retrieving the weather data

const int httpPort = 80;
String result;
int cityIDLoop = 0;

// a list of cities you want to display the forecast for
// get the ID at https://openweathermap.org/
// type the city, click search and click on the town
// then check the link, like this: https://openweathermap.org/city/5128581
// 5128581 is the ID for New York

String cityIDs[] = {
   "1261481",  // New Delhi
  "1273313",   // Dehradun
  "1264527",  // Chennai
  "1275339",   // Mumbai
  "1255634"  // Srinagar

};

// 
// settings
//

int startupDelay = 1000;                      // startup delay
int loopDelay = 3000;                         // main loop delay between sensor updates

int timeServerDelay = 1000;                   // delay for the time server to reply
int timeServerPasses = 4;                     // number of tries to connect to the time server before timing out
boolean timeServerConnected = false;          // is set to true when the time is read from the server

int maxForecastLoop = 10;                     // number of main loops before the forecast is refreshed, looping through all cities
int weatherForecastLoop = 0;
int weatherForecastLoopInc = 1;

int displayStartupDimValue = 30;              // startup display backlight level
int displayDimValue = 150;                    // main display backlight level
int displayDimStep = 1;                       // dim step
int dimStartupDelay = 50;                     // delay for fading in
int dimPageDelay = 0;                         // delay for fading between pages

//
// initialize variables
//

String command;
String doubleQuote = "\"\"";


//
// initialize timer
//

os_timer_t secTimer;
void endNextionCommand() {
  nexSerial.write(0xff);
  nexSerial.write(0xff);
  nexSerial.write(0xff);
}

void printNextionCommand (String command) {
  nexSerial.print(command);
  endNextionCommand();
}

void sendToLCD(uint8_t type,String index, String cmd) {
  if (type == 1 ) {
    nexSerial.print(index);
    nexSerial.print(".txt=");
    nexSerial.print("\"");
    nexSerial.print(cmd);
    nexSerial.print("\"");
  }
  else if (type == 2) {
    nexSerial.print(index);
    nexSerial.print(".val=");
    nexSerial.print(cmd);
  }
  else if (type == 3) {
    nexSerial.print(index);
    nexSerial.print(".pic="); 
    nexSerial.print(cmd);
  }
  else if (type ==4 ) {
    nexSerial.print("page ");
    nexSerial.print(cmd);
  }

  endNextionCommand();
}


void displayFadeIn(int fromValue, int toValue, int fadeDelay) {
  for (int i = fromValue; i <= toValue; i += displayDimStep) {
    if (i > toValue)
      i = toValue;
    printNextionCommand("dim=" + String(i));
    delay(fadeDelay);
  }
}

void displayFadeOut(int fromValue, int fadeDelay) {
  for (int i = fromValue; i >= 0; i -= displayDimStep) {
    if (i < 0)
      i = 0;
    printNextionCommand("dim=" + String(i));
    delay(fadeDelay);
  }
}
String getShortWindDirection (int degrees) {
  int sector = ((degrees + 11) / 22.5 - 1);
  switch (sector) {
    case 0: return "N"; 
    case 1: return "NNE";
    case 2: return "NE";
    case 3: return "ENE";
    case 4: return "E";
    case 5: return "ESE";
    case 6: return "SE";
    case 7: return "SSE";
    case 8: return "S";
    case 9: return "SSW";
    case 10: return "SW";
    case 11: return "WSW";
    case 12: return "W";
    case 13: return "WNW";
    case 14: return "NW";
    case 15: return "NNW";
  }
}

void setWeatherPicture(int weatherID) {
 switch(weatherID)
 {
  case 200:
  case 201:
  case 202:
  case 210: sendToLCD(3, "weatherpic", "26"); break; // tstorm1
  case 211: sendToLCD(3, "weatherpic", "27"); break; // tstorm2
  case 212: sendToLCD(3, "weatherpic", "28"); break; // tstorm3
  case 221:
  case 230:
  case 231:
  case 232: sendToLCD(3, "weatherpic", "27"); break; // tstorm2

  case 300:
  case 301:
  case 302: 
  case 310: 
  case 311: 
  case 312: 
  case 313: 
  case 314: 
  case 321: sendToLCD(3, "weatherpic", "15"); break; // rain1

  case 500: 
  case 501: sendToLCD(3, "weatherpic", "15"); break; // rain1
  case 502: 
  case 503: 
  case 504: sendToLCD(3, "weatherpic", "16"); break; // rain2
  case 511: 
  case 520: 
  case 521: sendToLCD(3, "weatherpic", "17"); break; // shower1
  case 522: 
  case 531: sendToLCD(3, "weatherpic", "18"); break; // shower2

  case 600: sendToLCD(3, "weatherpic", "20"); break; // snow1
  case 601: sendToLCD(3, "weatherpic", "22"); break; // snow3
  case 602: sendToLCD(3, "weatherpic", "24"); break; // snow5
  case 611: 
  case 612: sendToLCD(3, "weatherpic", "14"); break; // sleet
  case 615: sendToLCD(3, "weatherpic", "20"); break; // snow1
  case 616: sendToLCD(3, "weatherpic", "22"); break; // snow3
  case 620: sendToLCD(3, "weatherpic", "20"); break; // snow1
  case 621: sendToLCD(3, "weatherpic", "22"); break; // snow3
  case 622: sendToLCD(3, "weatherpic", "24"); break; // snow5

  case 701: 
  case 711: 
  case 721: sendToLCD(3, "weatherpic", "13"); break; // mist
  case 731: sendToLCD(3, "weatherpic", "10"); break; // dunno
  case 741: sendToLCD(3, "weatherpic", "11"); break; // fog
  case 751: 
  case 761: 
  case 762: 
  case 771: 
  case 781: sendToLCD(3, "weatherpic", "10"); break; // dunno

  case 800: sendToLCD(3, "weatherpic", "25"); break; // sunny
  case 801: sendToLCD(3, "weatherpic", "5"); break; // cloud1
  case 802: sendToLCD(3, "weatherpic", "7"); break; // cloud3
  case 803: sendToLCD(3, "weatherpic", "8"); break; // cloud4
  case 804: sendToLCD(3, "weatherpic", "14"); break; // overcast

  case 906: sendToLCD(3, "weatherpic", "12"); break; // hail
  
  default: sendToLCD(3, "weatherpic", "10"); break; // dunno
 }
} 

void delayCheckTouch (int delayTime) {
  unsigned long startMillis = millis();

  while (millis() - startMillis < delayTime) {
    delay(1000);
  }
}

String dayAsString(int day) {
  switch (day) { 
    case 1: return "Sunday";
    case 2: return "Monday";
    case 3: return "Tuesday";
    case 4: return "Wednessday";
    case 5: return "Thursday";
    case 6: return "Friday";
    case 7: return "Saturday";
  }
  return "" ;
}

String monthAsString(int month) {
  switch (month) {
    case 1:  return "January";
    case 2:  return "February";
    case 3:  return "March";
    case 4:  return "April";
    case 5:  return "May";
    case 6:  return "June";
    case 7:  return "July";
    case 8:  return "August";
    case 9:  return "September";
    case 10: return "October";
    case 11: return "November";
    case 12: return "December";
  }
  return "" ;
}

void displayDate() {
  time_t t = now();
  
  command = "date.txt=\"" + String(day(t)) + " " + monthAsString(month(t)) + "\"";
  printNextionCommand(command);
  command = "year.txt=\"" + String(year(t)) + "\"";
  printNextionCommand(command);
}

void displayTime() {
  time_t t = now();
  char timeString[9];
  
  snprintf(timeString, sizeof(timeString), "%02d:%02d:%02d", hour(t), minute(t), second(t));
  command = "time.txt=\"" + String(timeString) + "\"";
  printNextionCommand(command);
}

void timerDisplayTime(void *pArg) {
  displayTime();
}

//
// setup
//

//
// functions
//



//
// network functions
//

void connectToWifi() {
  int wifiBlink = 0;

//  WiFi.enableSTA(true);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    if (wifiBlink == 0) {
      printNextionCommand("wifi_connect.txt=\"connect...\"");
      wifiBlink = 1;
    }
    else {
      printNextionCommand("wifi_connect.txt=" + doubleQuote);
      wifiBlink = 0;
    }
    delay(500);
  }

  printNextionCommand("wifi_connect.txt=" + doubleQuote);
}
unsigned long sendNTPpacket(IPAddress& address) {
  memset(packetBuffer, 0, 48);
  packetBuffer[0] = 0b11100011;     // LI, Version, Mode
  packetBuffer[1] = 0;              // Stratum, or type of clock
  packetBuffer[2] = 6;              // Polling Interval
  packetBuffer[3] = 0xEC;           // Peer Clock Precision
                                    // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  clockUDP.beginPacket(address, 123);    //NTP requests are to port 123
  clockUDP.write(packetBuffer, 48);
  clockUDP.endPacket();
}

//
// get and display weather data
//


void getTimeFromServer() {
  #ifdef DEBUG
    Serial.print("Getting time from server...");
  #endif
  
  int connectStatus = 0, i = 0;
  unsigned long unixTime;

  while (i < timeServerPasses && !connectStatus) {
    #ifdef DEBUG
      Serial.print(i);
      Serial.print("...");
    #endif

    printNextionCommand("time.txt=\"get time..\"");
    WiFi.hostByName(ntpServerName, timeServerIP); 
    sendNTPpacket(timeServerIP);
    delay(timeServerDelay / 2);
    connectStatus = clockUDP.parsePacket();
    printNextionCommand("time.txt=" + doubleQuote);
    delay(timeServerDelay / 2);
    i++;
  }
    
  if (connectStatus) {  
    #ifdef DEBUG
      Serial.print(i);
      Serial.println("...connected");
    #endif

    timeServerConnected = true;
    clockUDP.read(packetBuffer,48);
  
    // the timestamp starts at byte 40 of the received packet and is four bytes, or two words, long.
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // the timestamp is in seconds from 1900, add 70 years to get Unixtime
    unixTime = (highWord << 16 | lowWord) - 2208988800 + timeZoneoffsetGMT;
    
    if (DST)
      unixTime = unixTime + 3600;
  
    setTime(unixTime);
  }
  else {
    #ifdef DEBUG
      Serial.print(i);
      Serial.println("...failed...");
    #endif

    printNextionCommand("time.txt=\"failed....\"");
    delay(timeServerDelay);
    printNextionCommand("time.txt=" + doubleQuote);
  }
}

void getWeatherData() //client function to send/receive GET request data.
{
  WiFiClient client;
  if (!client.connect(servername, httpPort))
    return;

  String cityID = cityIDs[cityIDLoop];
  cityIDLoop++; 
  
  if (cityIDs[cityIDLoop] == "")
    cityIDLoop = 0;  
    
  String url = "/data/2.5/forecast?id=" + cityID + "&units=metric&cnt=1&APPID=" + APIKEY;
  //String url = "/data/2.5/weather?id=" + cityID + "&units=metric&cnt=1&APPID=" + APIKEY;
  //check weather properties at https://openweathermap.org/current

  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + servername + "\r\n" + "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      client.stop();
      return;
    }
  }

  result = "";
  // Read all the lines of the reply from server
  while(client.available()) {
      result = client.readStringUntil('\r');
  }

  result.replace('[', ' ');
  result.replace(']', ' ');
  
  char jsonArray [result.length()+1];
  result.toCharArray(jsonArray,sizeof(jsonArray));
  jsonArray[result.length() + 1] = '\0';
  
  StaticJsonBuffer<1024> json_buf;
  JsonObject &root = json_buf.parseObject(jsonArray);
  if (!root.success())
    nexSerial.println("parseObject() failed");

  //check properties forecasts at https://openweathermap.org/forecast5

  int weatherID = root["list"]["weather"]["id"];
  
  String tmp0 = root["city"]["name"]; 
  String tmp1 = root["list"]["weather"]["main"]; 
  String tmp2 = root["list"]["weather"]["description"]; 
  float  tmp3 = root["list"]["main"]["temp_min"]; 
  float  tmp4 = root["list"]["main"]["temp_max"]; 
  float  tmp5 = root["list"]["main"]["humidity"]; 
  float  tmp6 = root["list"]["clouds"]["all"]; 
  float  tmp7 = root["list"]["rain"]["3h"]; 
  float  tmp8 = root["list"]["snow"]["3h"]; 
  float  tmp9 = root["list"]["wind"]["speed"]; 
  int    tmp10 = root["list"]["wind"]["deg"]; 
  float  tmp11 = root["list"]["main"]["pressure"]; 
//String tmp12 = root["list"]["dt_text"]; command = command + tmp12;

  #if !defined (DEBUG_NO_PAGE_FADE)
    displayFadeOut(displayDimValue, dimPageDelay);
  #endif
  #if !defined (DEBUG_NO_PAGE_FADE)
    displayFadeIn(0, displayDimValue, dimPageDelay );
  #endif

  setWeatherPicture(weatherID);
  sendToLCD(1, "city", tmp0);
  sendToLCD(1, "description", tmp2);
  sendToLCD(1, "humidity", String(tmp5, 0));
  sendToLCD(1, "rain", String(tmp7, 1));
  sendToLCD(1, "wind_dir", getShortWindDirection(tmp10));
  sendToLCD(1, "wind_speed", String(tmp9,1));
  sendToLCD(1, "pressure", String(tmp11, 0));
  sendToLCD(1, "clouds", String(tmp6, 0));
  sendToLCD(1, "temp_min", String(tmp3, 1));
  sendToLCD(1, "temp_max", String(tmp4, 1));

  //sendToLCD(1, "weather_ID", String(weatherID, 0));
}

String getWindDirection (int degrees) {
  int sector = ((degrees + 11) / 22.5 - 1);
  switch (sector) {
    case 0: return "north"; 
    case 1: return "nort-northeast";
    case 2: return "northeast";
    case 3: return "east-northeast";
    case 4: return "east";
    case 5: return "east-southeast";
    case 6: return "southeast";
    case 7: return "south-southeast";
    case 8: return "south";
    case 9: return "south-southwest";
    case 10: return "southwest";
    case 11: return "west-southwest";
    case 12: return "west";
    case 13: return "west-northwest";
    case 14: return "northwest";
    case 15: return "north-northwest";
  }
}

void setup() {
  #ifdef DEBUG
    Serial.begin(9600);
  #endif
  
  nexSerial.begin(9600);

  printNextionCommand("dims=" + String(0));         // set initial startup backlight value of the Nextion display to 0
  printNextionCommand("dim=" + String(0));          // also set the current backlight value to 0
  printNextionCommand("page 1");

  delay(startupDelay);
  
  displayFadeIn(0, displayStartupDimValue, dimStartupDelay);

  connectToWifi();
  clockUDP.begin(localPort);
  getTimeFromServer();

    
  displayFadeIn(displayStartupDimValue, displayDimValue, dimStartupDelay / 2);

  #ifdef DEBUG
    Serial.println("Starting main loop");
  #endif
}

//
// main loop
//

void loop() {
  if (!timeServerConnected) {
    getTimeFromServer();
    //if (timeServerConnected)
      //os_timer_arm(&secTimer, 1000, true);
  }
   else
      delayCheckTouch(loopDelay);
      displayTime();
      displayDate();
  #ifdef DEBUG
     Serial.print(millis()/1000);
    Serial.println("");
  #endif
  getWeatherData();  
}


