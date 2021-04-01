#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Adafruit_ILI9341.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <SFE_BMP180.h>
#include <Wire.h>

// Replace with your network credentials
const char* ssid = "skaza-guest";
const char* password = "";

// Ura
#define utcoffset 3600   // doda časovni pas 1h=3600
char daysOfTheWeek[7][12] = {"Nedelja    ", "Ponedeljek ", "Torek     ", "Sreda     ", "Cetrtek    ", "Petek     ", "Sobota     "};

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp1.arnes.si", utcoffset);

// Senzor Tlaka BMP180
// You will need to create an SFE_BMP180 object, here called "pressure":
/* 
Hardware connections:
Arduino pins labeled:  SDA  SCL   VCC   GND
ESP8266 Mini D1        D2   D1    3V3   GND
*/
SFE_BMP180 pressure;

#define ALTITUDE 418.0 // Altitude in meters

// Senzor temperature in vlage DHT22
// Uncomment the type of sensor in use:
//#define DHTTYPE    DHT11     // DHT 11
#define DHTTYPE    DHT22     // DHT 22 (AM2302)
//#define DHTTYPE    DHT21     // DHT 21 (AM2301)

#define DHTPIN D6     // Digital pin connected to the DHT sensor

DHT dht(DHTPIN, DHTTYPE);


// TFT Displey
// Priklop TFT Displeja na ESP8266 D1 Mini
// VCC ---------> 3V3
// GND ---------> GND
// CS  ---------> D8
// RST ---------> D3
// DC  ---------> D1
// SDI(MOSI)----> D7
// SCK ---------> D5
// LED ---------> 3V3
#define TFT_CS    D8     // TFT CS  pin is connected to NodeMCU pin D2
#define TFT_RST   D3     // TFT RST pin is connected to NodeMCU pin D3
#define TFT_DC    D4     // TFT DC  pin is connected to NodeMCU pin D4

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// current temperature & humidity, updated in loop()
float t = 0.0;
float h = 0.0;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;    // will store last time DHT was updated

// Updates DHT readings every 10 seconds
const long interval = 500;  

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
  </style>
</head>
<body>
  <h2>Temperatura in Vlaga Pisarna VZD</h2>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="dht-labels">Temperatura:</span> 
    <span id="temperature">%TEMPERATURE%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-tint" style="color:#00add6;"></i> 
    <span class="dht-labels">Vlaga:</span>
    <span id="humidity">%HUMIDITY%</span>
    <sup class="units">%</sup>
  </p>
</body>
<script>
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperature").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperature", true);
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("humidity").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/humidity", true);
  xhttp.send();
}, 10000 ) ;
</script>
</html>)rawliteral";

// Replaces placeholder with DHT values
String processor(const String& var){
  //Serial.println(var);
  if(var == "TEMPERATURE"){
    return String(t);
  }
  else if(var == "HUMIDITY"){
    return String(h);
  }
  return String();
}

//Grafika
  void drawPictureFrames() {
  tft.setRotation(0);               //Obračanje displeja
  tft.fillScreen(ILI9341_BLUE);    //Barva ozadja

  tft.setTextSize(1);

  // Table
  tft.drawRect(0, 0, 240, 320, ILI9341_WHITE);
  tft.drawLine(0, 80, 240, 80, ILI9341_WHITE);
  tft.drawLine(0, 160, 240, 160, ILI9341_WHITE);
  tft.drawLine(0, 240, 240, 240, ILI9341_WHITE);

  //Napis v 2 kvadratu temperatura
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(15, 90);
  tft.print("Temperatura");

  // Znak stopinje
  tft.drawCircle(198, 123, 4, ILI9341_WHITE);
  tft.drawCircle(198.25, 123.25, 3, ILI9341_WHITE);  
  tft.drawCircle(198.5, 123.5, 2, ILI9341_WHITE);
  
  //Znak Celzij
  tft.setTextSize(4);
  tft.setCursor(206, 123);
  tft.print("C");

   //Napis v 3 kvadratu vlaga
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(15, 170);
  tft.print("Vlaga");

  //Znak %
  tft.setTextSize(4);
  tft.setCursor(170, 203);
  tft.print("%");

   //Napis v 4 kvadratu tlak
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(15, 250);
  tft.print("Tlak");

  //Znak mBar
  tft.setTextSize(2);
  tft.setCursor(182, 293);
  tft.print("mBar");
}

void setup(){
    // Initialize the sensor (it is important to get calibration values stored on the device).

  if (pressure.begin())
    Serial.println("BMP180 init success");
  else
  {
    // Oops, something went wrong, this is usually a connection problem,
    // see the comments at the top of this sketch for the proper connections.

    Serial.println("BMP180 init fail\n\n");
    while(1); // Pause forever.
  }
   
   
   //URA void setup
   timeClient.begin();
  
  // Serial port for debugging purposes
  Serial.begin(115200);
  dht.begin();

  tft.begin();
  drawPictureFrames();
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println(".");
  }

  // Print ESP8266 Local IP Address
  Serial.println(WiFi.localIP());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(t).c_str());
  });
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(h).c_str());
  });

  // Start server
  server.begin();
}
 
void loop(){

  //URA void loop
  timeClient.update();

  // Beri dan
  tft.setTextSize(3);
  tft.setCursor(25, 7);
  tft.setTextColor(ILI9341_GREEN,ILI9341_BLUE );
  tft.print(daysOfTheWeek[timeClient.getDay()]);
   
  // Beri URO
  tft.setTextSize(4);
  tft.setCursor(25, 42);
  tft.setTextColor(ILI9341_GREEN,ILI9341_BLUE );
  tft.print(timeClient.getFormattedTime());

  Serial.println(daysOfTheWeek[timeClient.getDay()]);
  Serial.println(timeClient.getFormattedTime());

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    // save the last time you updated the DHT values
    previousMillis = currentMillis;
  
  // BMP180
        char status;
        double T1,Pr,p0,a;
      
          status = pressure.startTemperature();
        if (status != 0)
        {
          // Wait for the measurement to complete:
          delay(status);
          status = pressure.getTemperature(T1);
          if (status != 0)
          {
            status = pressure.startPressure(3);
            if (status != 0)
            {
              // Wait for the measurement to complete:
              delay(status);
              status = pressure.getPressure(Pr,T1);
              if (status != 0)
              {
                // Print out the measurement:
                p0 = pressure.sealevel(Pr,ALTITUDE); // we're at 1655 meters (Boulder, CO)
                Serial.println(p0,1);
               }
            }
          }
        }

    // Beri Tlak
    tft.setTextSize(5);
    tft.setCursor(52, 273);
    tft.setTextColor(ILI9341_WHITE,ILI9341_BLUE );
    tft.print(p0,0);
   }

  // DHT22
    // Read temperature as Celsius (the default)
    float newT = dht.readTemperature();
    // Read temperature as Fahrenheit (isFahrenheit = true)
    //float newT = dht.readTemperature(true);
    // if temperature read failed, don't change t value
    if (isnan(newT)) {
      Serial.println("Failed to read from DHT sensor!");
    }
    else {
      t = newT;
        //DHT temperatura
       tft.setTextSize(5);
       tft.setCursor(58, 115);
       tft.setTextColor(ILI9341_WHITE,ILI9341_BLUE );
       tft.print(t,1);
      Serial.println(t);
    }
    // Read Humidity
    float newH = dht.readHumidity();
    // if humidity read failed, don't change h value 
    if (isnan(newH)) {
      Serial.println("Failed to read from DHT sensor!");
     }
    else {
      h = newH;
        //DHT vlaga
       tft.setTextSize(5);
       tft.setCursor(87, 195);
       tft.setTextColor(ILI9341_WHITE,ILI9341_BLUE );
       tft.print(h,0);
      Serial.println(h);
      }

}
