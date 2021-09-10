/*
 Simple Smart Whole House Fan Timer Controller

 Controlled via physical buttons or web user interface. 

 Designed for ESP8266 based boards like the Wemos D1 Mini or NodeMCU
 
 To install the ESP8266 board, (using Arduino 1.6.4+):
  - Add the following 3rd party board manager under "File -> Preferences -> Additional Boards Manager URLs":
       http://arduino.esp8266.com/stable/package_esp8266com_index.json
  - Open the "Tools -> Board -> Board Manager" and click install for the ESP8266"
  - Select your ESP8266 in "Tools -> Board"

 Dependancies include:
   - WiFiManager (by TZAPU) (https://github.com/tzapu/WiFiManager)
   - Adafruit SSD1306 for the OLED display
   - Adafruit GFX library for graphics manipulations / fonts 
   - ArduinoJSON for parsing API inputs / outputs
   - PubSubClient for MQTT Capabilities

 Initial Wifi Setup: 
  To connect your device to WiFi, use a smartphone or computer with WiFi to connect to the board AP (SSID: Smart-WHF-AP).
  Then open a broswer and navigate to 192.168.4.1 to configure the device for WiFi. 
*/

#define mqttenabled // Comment out this line to disable MQTT support

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>   // Include the WebServer library
#include <WiFiManager.h> 
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <ArduinoJson.h>

#ifdef mqttenabled
// If MQTT enabled, then setup mqtt
#include <PubSubClient.h>

// Edit these values for your implementation
char defaultmqttServer[] = "192.168.10.46";  // put your MQTT Server Address here
char defaultmqttPort[] = "1883"; // put your MQTT Server Port here
char defaultmqttUser[] = "mqtt-user"; // put your MQTT Server Username here
char defaultmqttPassword[] = "password"; // put your MQTT Server Password here
// End value edit
String mqttServer = String(defaultmqttServer);
int mqttPort = atoi(defaultmqttPort);
String mqttUser = String(defaultmqttUser);
String mqttPassword = String(defaultmqttPassword);
const char* mqttCommandTopic = "whfan/on/set";
const char* mqttPresetCommandTopic = "whfan/preset/set";
const char* mqttPresetStateTopic = "whfan/preset/state";
const char* mqttStateTopic = "whfan/on/state";
const char* mqttAvailTopic = "whfan/on/avail";
const char* mqttSpeedStateTopic = "whfan/speed/state";
const char* mqttSpeedCommandTopic = "whfan/speed/set";
const char* mqttPercentageStateTopic = "whfan/percentage/state";
const char* mqttPercentageCommandTopic = "whfan/percentage/set";
unsigned long mqttLastCheck; 
WiFiClient espClient;
PubSubClient client(espClient);
#endif 

const char* wifiHostname = "whfan";
ESP8266WebServer server(80);    // Create a webserver object that listens for HTTP request on port 80

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define DEBOUNCE 250 // Millisecond debounce for buttons
#define REFRESH 200 // Millisecond refresh for display 
#define GPIO_timebtn 0 // Input button for time selection
#define GPIO_speedbtn 2 // Input button for speed selection
#define GPIO_HIGHrelay 14 // High Fan Speed Relay Output Enable
#define GPIO_LOWrelay 16 // Low Fan Speed Relay Output Enable

int state = 0;  // Current Fan Speed / State : 0 = Off, 1 = High, 2 = Low
int iconstate = 0;  // Current Icon Animation Image : 0 = Starting Position, 1-2 = 22.5d rotated, 3-4 = 45d rotated, 5-6 = 67.5d rotated
bool holdmode = false; // Indicates hold mode
bool lockmode = false; // Indicates lock mode (physical buttons are locked)
bool lockinitiated = false;  // Indicates unlock / lock mode has been initiated
unsigned long locktimer = 0; // Tracks start time of lock / unlock events
unsigned long start_time = 0;   // Track start time of the fan
unsigned long runtime = 0;  // Track total runtime
unsigned long timebtnlastpress = 0;  // Millis when the time button was last pressed, for debounce
unsigned long spdbtnlastpress = 0;  // Millis when the speed button was last pressed, for debounce
unsigned long displayrefreshlast = 0;  // Millis when the screen was last refreshed
unsigned long now = 0; // Tracks the current millis output

String timestring = "";  // Time String to display

String favicon = "<link rel=\"shortcut icon\" href=\"data:image/x-icon;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAAXNSR0IArs4c6QAAAkpJREFUOE99kz9IclEYxp9r/iPExaGgoE2LlHCpFHQIjIIQjGpJcdAhCBwcFRuzljZxyEk3URQcKqIhHOQKhUggGiEiijopggSKN865edEv+d7lnj/P+7vnfc57GI7jOPwGGRYKBUgkEmi12snyf7/MBNBqtXB2doZsNguGYXB/fw+XyyUkt9ttvL6+YmVlBUajkWpIUMBoNMLu7i7e3t6EhKOjI2QyGTrP5XI4PDxEr9ejczKOx+NQKBQ84Pn5Gfv7+zNHPT4+RjKZxPf3N9RqNer1+sy+w+FANBrlAUR4cnIyI9jb28PLywuF/LtHhAsLC+h0Ojyg2+1S0xqNhgAhRl5fX1PI4+PjHyNlMhmazSYPILufn5+w2+3I5/NUbLVaEQwGMRwOcXFxgWKxCJPJBLlcjvF4DLfbTTUCgCQR1vv7O+7u7uD3+2E2m6FSqRCJREBuYV4pM4DJOa+urqj49PQUy8vL8Pl8iMViOD8/h8VigVgsFkpixuMxFwgEkE6nIZVKQfyoVqswGAy4vb3FYDCAx+NBpVKhSZubm0gkElhfX+f7gGVZbmdnZ263eb1emkDqnWpYrK2toVwugxjJlEolbmNjYy6gVCrRP2k0GuEEEyHLstje3uZNtNlstITpODg4wMPDA13S6/X0jUxCJBKhVqthdXWVB/T7fTidTqRSKarZ2trC09MTlpaW6Fyn0+Hj40MAkGsNh8O8B9Ov8evri5pIANNOh0IhaiRprsvLS9zc3NDxH8BcI34XyWtdXFyEUqmckf0AN/wOdCsRyqUAAAAASUVORK5CYII=\" />";

// Fan Icon Animation - 24x24 ICON rotates 22.5 degrees per step
const unsigned char fanicon0 [] PROGMEM = {
  0x00, 0x3e, 0x00, 0x00, 0x7f, 0x80, 0x00, 0xff, 0x80, 0x00, 0xff, 0x80, 0x00, 0xff, 0x80, 0x00, 
  0xff, 0x00, 0x00, 0x7e, 0x00, 0x78, 0x7c, 0x00, 0x7c, 0x3c, 0x38, 0xfe, 0x66, 0xfe, 0xff, 0xd3, 
  0xfe, 0xff, 0xb9, 0xff, 0xff, 0x99, 0xff, 0xff, 0xc3, 0xff, 0x7f, 0x64, 0x7f, 0x1c, 0x3c, 0x3e, 
  0x00, 0x3e, 0x1e, 0x00, 0x7e, 0x00, 0x00, 0xff, 0x00, 0x01, 0xff, 0x00, 0x01, 0xff, 0x00, 0x01, 
  0xfe, 0x00, 0x01, 0xfe, 0x00, 0x00, 0x78, 0x00
};

const unsigned char fanicon1 [] PROGMEM = {
  0x00, 0xf0, 0x00, 0x01, 0xf8, 0x00, 0x03, 0xfc, 0x00, 0x07, 0xf8, 0x00, 0x07, 0xf8, 0x00, 0x07, 
  0xf8, 0x38, 0x03, 0xf0, 0xfc, 0x03, 0xf1, 0xfe, 0x01, 0xfb, 0xff, 0x00, 0xc7, 0xff, 0x30, 0x5b, 
  0xff, 0xfc, 0xb9, 0xff, 0xff, 0xb9, 0x3e, 0xff, 0xc2, 0x04, 0xff, 0xe7, 0x00, 0xff, 0x9f, 0x80, 
  0x7f, 0x8f, 0xc0, 0x3f, 0x0f, 0xc0, 0x1c, 0x1f, 0xe0, 0x00, 0x1f, 0xe0, 0x00, 0x3f, 0xe0, 0x00, 
  0x3f, 0xc0, 0x00, 0x1f, 0x80, 0x00, 0x0e, 0x00
};

const unsigned char fanicon2 [] PROGMEM = {
  0x00, 0x00, 0x00, 0x03, 0x80, 0x00, 0x0f, 0xc0, 0x40, 0x1f, 0xc1, 0xf0, 0x1f, 0xc3, 0xf8, 0x1f, 
  0xc7, 0xfc, 0x3f, 0xc7, 0xfe, 0x1f, 0xc7, 0xfe, 0x1f, 0xff, 0xfc, 0x0f, 0xc7, 0xfc, 0x03, 0x9b, 
  0x00, 0x00, 0xb8, 0x00, 0x00, 0xb9, 0x00, 0x01, 0xc3, 0xc0, 0x3f, 0xe7, 0xf0, 0x7f, 0xff, 0xf8, 
  0x7f, 0xe3, 0xf8, 0x7f, 0xe3, 0xf8, 0x3f, 0xc3, 0xf8, 0x1f, 0xc3, 0xf8, 0x0f, 0x83, 0xf0, 0x00, 
  0x03, 0xe0, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00
};

const unsigned char fanicon3 [] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x07, 0xc0, 0x00, 0x0f, 0xe0, 0x08, 0x1f, 0xf0, 0x3e, 0x1f, 0xf0, 0x3e, 
  0x1f, 0xf0, 0x7f, 0x1f, 0xf0, 0x7f, 0x1f, 0xc0, 0x7f, 0x9f, 0x00, 0x7f, 0xc6, 0x00, 0x7f, 0x9a, 
  0x00, 0x3f, 0xb9, 0xf8, 0x0f, 0xb9, 0xfc, 0x00, 0x43, 0xfe, 0x00, 0x67, 0xfe, 0x00, 0xf9, 0xfe, 
  0x03, 0xf8, 0xfe, 0x0f, 0xf8, 0xfe, 0x1f, 0xf8, 0x7c, 0x1f, 0xf8, 0x78, 0x0f, 0xf0, 0x00, 0x07, 
  0xf0, 0x00, 0x03, 0xe0, 0x00, 0x00, 0x00, 0x00
};

void handleRoot(); // function prototypes for HTTP handlers
void handleNotFound();  // 404 not found
void handleAPI();     // function for handling API call
void handleAddTime1();   // function for adding 1 hour to time
void handleAddTime2();   // function for adding 2 hours to time
void handleHoldMode();   // function for setting HOLD mode
void handleChangeSpeed();  // function to change fan speed High - Low - Off 
void handleRefreshData();  // function to handle refresh data for main page
void handleDiag(); // function for display diagnostic screen
void handleRestart(); // function for restarting the ESP
void handleToggleLock(); // function to toggle lock mode 

// This function's author is apicquot from https://forum.arduino.cc/index.php?topic=228884.0
String IpAddress2String(const IPAddress& ipAddress)
{
    return String(ipAddress[0]) + String(".") +
           String(ipAddress[1]) + String(".") +
           String(ipAddress[2]) + String(".") +
           String(ipAddress[3]);
}

void setup(void){
  String event;
  Serial.begin(115200); // Start the Serial communication to send messages to the computer
  // Setup GPIO Pins
  pinMode(GPIO_timebtn, INPUT_PULLUP);     // Input for button 1  (timebtn)
  pinMode(GPIO_speedbtn, INPUT_PULLUP);     // Input for button 2  (speedbtn)
  pinMode(GPIO_HIGHrelay, OUTPUT);  // Output for HIGH relay
  pinMode(GPIO_LOWrelay, OUTPUT);  // Output for LOW relay

  digitalWrite(GPIO_HIGHrelay, HIGH);  // Turn OFF HIGH relay
  digitalWrite(GPIO_LOWrelay, HIGH);  // Turn OFF LOW relay

  delay(10);
  Serial.println('\n');

  // **** Init Display
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.clearDisplay();
  display.display();

  display.setFont();
  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);
  display.println("Setting Up WiFi...");
  display.setCursor(0,10); 
  display.println("Starting AP Mode...");
  display.display();

  // **** Init Wifi / Wifi Manager
  WiFiManager wifiManager;

  #ifdef mqttenabled
  // Add mqtt server to the WiFi Manager Settings
  // id/name, placeholder/prompt, default, length
  WiFiManagerParameter your_mqtt_server("mserver", "MQTT Server", defaultmqttServer, 40);
  WiFiManagerParameter your_mqtt_port("mport", "MQTT Port", defaultmqttPort, 6);
  WiFiManagerParameter your_mqtt_user("muser", "MQTT Username", defaultmqttUser, 25);
  WiFiManagerParameter your_mqtt_password("mpass", "MQTT Password", defaultmqttPassword, 40);
  
  wifiManager.addParameter(&your_mqtt_server);
  wifiManager.addParameter(&your_mqtt_port);
  wifiManager.addParameter(&your_mqtt_user);
  wifiManager.addParameter(&your_mqtt_password);
  
  #endif
  
  wifiManager.autoConnect("Smart-WHF-AP");
  wifiManager.setHostname(wifiHostname);

  #ifdef mqttenabled
  // Get values for custom setup
  mqttServer = your_mqtt_server.getValue();
  String mqttPortStrPlaceHolder = your_mqtt_port.getValue();
  mqttPort = mqttPortStrPlaceHolder.toInt();
  mqttUser = your_mqtt_user.getValue();
  mqttPassword = your_mqtt_password.getValue();
  #endif
  
  event = "Connected to " + WiFi.SSID();
  Serial.println(event);              // Tell us what network we're connected to
  display.setCursor(0,20);
  display.println(event);
  
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());           // Send the IP address of the ESP8266 to the computer

  display.setCursor(0,30); 
  display.println(WiFi.localIP());
  display.display();


  if (MDNS.begin("smartwhf")) {              // Start the mDNS responder for smartwhf.local
    event = "mDNS responder started";
  } else {
    event = "Error setting up MDNS responder!";
  }
  Serial.println(event);
  display.setCursor(0,40); 
  display.println(event);
  display.display();

  // Setup HTTP Server

  server.on("/", handleRoot);               // Call the 'handleRoot' function when a client requests URI "/"
  server.on("/api", handleAPI);
  server.on("/addtime1", handleAddTime1);
  server.on("/addtime2", handleAddTime2);
  server.on("/holdmode", handleHoldMode);
  server.on("/changespeed", handleChangeSpeed);
  server.on("/refreshdata", handleRefreshData);
  server.on("/diag", handleDiag);
  server.on("/restart", handleRestart);
  server.on("/togglelock", handleToggleLock);

  server.onNotFound(handleNotFound);        // When a client requests an unknown URI (i.e. something other than "/"), call function "handleNotFound"

  server.begin();                           // Actually start the server
  Serial.println("HTTP server started");

  #ifdef mqttenabled
  // Start MQTT Initialization
  client.setServer(mqttServer.c_str(), mqttPort);
  client.setCallback(callback);

  delay(1000);
  display.clearDisplay();
  display.display();

  event = "Connecting to MQTT...";
  Serial.println(event);
  display.setCursor(0,0); 
  display.println(event);
  display.display();

  // Attempt to connect to MQTT Server 
  for(int i = 0; i <= 10; i++) {
    if (client.connect("smartwhfclient", mqttUser.c_str(), mqttPassword.c_str() )) {
      event = "MQTT Connected.";
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      event = "MQTT Failed.";
    }
  
    if(client.connected()) {
      client.subscribe(mqttCommandTopic);
      client.subscribe(mqttPresetCommandTopic);
      client.subscribe(mqttSpeedCommandTopic);
      client.subscribe(mqttAvailTopic);
      client.subscribe(mqttPercentageCommandTopic);
      mqttLastCheck = millis(); 
      // Publish initial state
      client.publish(mqttStateTopic, "OFF");
      client.publish(mqttPresetStateTopic, "off");
      client.publish(mqttAvailTopic, "Online");
      client.publish(mqttPercentageStateTopic, "0");
      break;
    }
    delay(2000);
  }

  Serial.println(event);
  display.setCursor(0,10); 
  display.println(event);
  display.display();
  #endif

  delay(1000);
  display.clearDisplay();
  display.display();
}

void loop(void){
  server.handleClient();  // Listen for HTTP requests from clients
  
  now = millis();

  #ifdef mqttenabled 
  // Check MQTT Connection (non-blocking)
  if ((!client.connected()) && (now - mqttLastCheck > 2000)) {
    reconnect();
    mqttLastCheck = now; 
  } else {
    client.loop();
  }
  #endif

  // Get physical input status  
  int timebtn = digitalRead(GPIO_timebtn);
  int speedbtn = digitalRead(GPIO_speedbtn);

  if (lockmode) {
    if ((timebtn == LOW) && (speedbtn == LOW) && (lockinitiated)) {
      // Wait for 3-Seconds to lock
      if ((now - locktimer) > 3000) {
        display.clearDisplay();
        display.setFont(&FreeSansBold12pt7b);
        display.setTextSize(1);             // Normal 1:1 pixel scale
        display.setTextColor(SSD1306_WHITE);        // Draw white text
        display.setCursor(40,20);
        display.println("UN-");
        display.setCursor(15,42);
        display.println("LOCKED");
        display.display();
        delay(2000);
        lockmode = false;
        locktimer = 0;
        lockinitiated = false;
        display.clearDisplay();
        display.display();
      }
    } else if ((timebtn == LOW) && (speedbtn == LOW) && !(lockinitiated)) {
      display.clearDisplay();
      display.setFont(&FreeSansBold12pt7b);
      display.setTextSize(1);             // Normal 1:1 pixel scale
      display.setTextColor(SSD1306_WHITE);        // Draw white text
      display.setCursor(15,20);
      display.println("LOCKED");
      display.setFont();
      display.setTextSize(1);             // Normal 1:1 pixel scale
      display.setCursor(30,50);
      display.println("Hold 3 Sec.");
      display.display();
      locktimer = now;
      lockinitiated = true; // 
    } else if ((timebtn == LOW) || (speedbtn == LOW)) {
      display.clearDisplay();
      display.setFont(&FreeSansBold12pt7b);
      display.setTextSize(1);             // Normal 1:1 pixel scale
      display.setTextColor(SSD1306_WHITE);        // Draw white text
      display.setCursor(15,20);
      display.println("LOCKED");
      display.display();
      delay(1000);
      display.clearDisplay();
      display.display();
      // if both buttons are not pressed, stay in lock mode
      lockinitiated = false; 
      locktimer = 0;
    } else {
      // if both buttons are not pressed, stay in lock mode
      lockinitiated = false; 
      locktimer = 0;
      // DISPLAY STATUS
      displayStatus();
    }
  } else {
    // If both buttons pressed, show status or lock
    if ((timebtn == LOW) && (speedbtn == LOW) && !(lockinitiated) && (locktimer == 0)) {
      locktimer = now;  // Start the lock timer
      Serial.print("Connected to ");
      Serial.println(WiFi.SSID());              // Tell us what network we're connected to
      Serial.print("IP address:\t");
      Serial.println(WiFi.localIP());           // Send the IP address of the ESP8266 to the computer

      display.clearDisplay();
      
      display.setFont();
      display.setTextSize(1);             // Normal 1:1 pixel scale
      display.setTextColor(SSD1306_WHITE);        // Draw white text
      display.setCursor(0,0);
      display.println(WiFi.SSID());
      display.setCursor(0,10); 
      display.println(WiFi.localIP());
      display.display();
    } else if ((timebtn == LOW) && (speedbtn == LOW) && !(lockinitiated) && (now - locktimer > 3000)) {
      lockinitiated = true;
      display.clearDisplay();
      display.setFont(&FreeSansBold12pt7b);
      display.setTextSize(1);             // Normal 1:1 pixel scale
      display.setTextColor(SSD1306_WHITE);        // Draw white text
      display.setCursor(10,20);
      display.println("LOCKING");
      display.display();
    } else if ((timebtn == LOW) && (speedbtn == LOW) && (lockinitiated) && (now - locktimer > 6000)) {
      lockinitiated = false;
      locktimer = 0;
      lockmode = true;
      display.clearDisplay();
      display.setFont(&FreeSansBold12pt7b);
      display.setTextSize(1);             // Normal 1:1 pixel scale
      display.setTextColor(SSD1306_WHITE);        // Draw white text
      display.setCursor(15,20);
      display.println("LOCKED");
      display.display();
      delay(2000);
      setOff();
    } else if ((timebtn == LOW) && (speedbtn == LOW)) {
      // Do nothing
      delay(10);
    } else {
      // Clear lock flags if set (i.e. someone let go of buttons)
      lockinitiated = false; 
      locktimer = 0;

      // DISPLAY STATUS
      displayStatus();

      // If Timer Button pressed:
      if ((timebtn == LOW) && (now - timebtnlastpress > DEBOUNCE)) {
        timebtnlastpress = now; 

        if(runtime < (4 * 60 * 60 * 1000)) {
          addTime(1);  // If less than four hours, add 1 hour to start time)
        } else {
          addTime(2);  // Else add 2 hours to start time)
        }
      }

      // If Speed Button pressed:
      if ((speedbtn == LOW) && (now - spdbtnlastpress > DEBOUNCE))  {
        state += 1;
        spdbtnlastpress = now;

        setSpeed();
      }
    }
  }
  delay (10);
}

void displayStatus() {
  // If display active, display status
  if(((start_time > 0) || (holdmode)) && (now - displayrefreshlast > REFRESH)) {
    displayrefreshlast = now;
    setTimestring();

    display.clearDisplay();
    
    display.setFont(&FreeSansBold12pt7b);
    display.setTextSize(1);             // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE);        // Draw white text
    display.setCursor(20,20);
    display.println(timestring);

    String statestring = "";
    if (state == 0) {
      statestring = "Off";
      iconstate = 0;
      display.setCursor(40,50);  
    }
    else if (state == 1) {
      statestring = "High";
      iconstate += 2;
      display.setCursor(36,50);  
    }
    else if (state == 2) {
      statestring = "Low";
      iconstate += 1;
      display.setCursor(40,50);  
    }
    display.println(statestring);

    if (iconstate == 0) {
      display.drawBitmap(128-24, 30, fanicon0, 24, 24, 1); 
    }
    else if (iconstate <=2) {
      display.drawBitmap(128-24, 30, fanicon1, 24, 24, 1); 
    }
    else if (iconstate <=4) {
      display.drawBitmap(128-24, 30, fanicon2, 24, 24, 1); 
    }
    else if (iconstate <=6) {
      display.drawBitmap(128-24, 30, fanicon3, 24, 24, 1); 
    }
    else {
      iconstate = 0;
      display.drawBitmap(128-24, 30, fanicon0, 24, 24, 1);
    }
    
    display.display();
  } else if ((start_time > 0) && (!holdmode) && (now - start_time > runtime)) {
    setOff();
  } 
}

void setTimestring() {
  if (holdmode) { 
    timestring = " HOLD ";
  } else if(((millis() - start_time)) < runtime) {
    timestring = "";
    if ((runtime - (millis() - start_time)) < (60 * 60 * 1000)) {  // If less than one hour display mins:secs 
      timestring += ((runtime - (millis() - start_time)) / 60000);
      timestring += "m ";
      timestring += ((runtime - (millis() - start_time)) / 1000) % 60;
      timestring += "s";
    } else if ((runtime - (millis() - start_time)) < (12 * 60 * 60 * 1000)) {  // If less than 10 hours display hours:mins 
      timestring += ((runtime - (millis() - start_time)) % (1000 * 60 * 60 * 24)) / (1000 * 60 * 60);  // Number of hours remaining
      timestring += "h ";
      timestring += ((runtime - (millis() - start_time)) % (1000 * 60 * 60)) / (1000 * 60); // Number of minutes remaining
      timestring += "m ";
    } else {  // Otherwise, show 12h+
      timestring += "12h+";
    }
  }
}

void setSpeed() {
// Function to cycle the speed (High -> Low -> Off ->)
  if (state > 2) {
    setOff();
  }
  else {
    if ((start_time == 0) && (!holdmode)) {
      start_time = millis();
      runtime += 1 * 60 * 60 * 1000;  // 1h * 60m * 60s * 1000ms (if timer hasn't been started then start it up)
      holdmode = false;
      #ifdef mqttenabled
      client.publish(mqttPresetStateTopic, "timer");
      #endif
    }
    if (state == 1) {
      digitalWrite(GPIO_LOWrelay, HIGH);  // Turn OFF LOW relay
      digitalWrite(GPIO_HIGHrelay, LOW);  // Turn ON HIGH relay
      #ifdef mqttenabled
      client.publish(mqttStateTopic, "ON");
      client.publish(mqttSpeedStateTopic, "high");
      client.publish(mqttPercentageStateTopic, "2");
      #endif
    }
    else {
      digitalWrite(GPIO_HIGHrelay, HIGH);  // Turn OFF HIGH relay
      digitalWrite(GPIO_LOWrelay, LOW);  // Turn ON LOW relay
      #ifdef mqttenabled
      client.publish(mqttStateTopic, "ON");
      client.publish(mqttSpeedStateTopic, "low");
      client.publish(mqttPercentageStateTopic, "1");
      #endif
    }

  }
}

void addTime(int addHours) {
// Function to add time to the timer

  if (holdmode) {
    // If in hold mode, then transition to off state
    setHold();
  } else {
    
    if ((start_time == 0) && (holdmode == false)) {
      start_time = millis();
      #ifdef mqttenabled
      client.publish(mqttPresetStateTopic, "timer");
      #endif
    }

    if (runtime < (10 * 60 * 60 * 1000)) {
      runtime += addHours * 60 * 60 * 1000;  // 1h * 60m * 60s * 1000ms  (Adding 1 hour to start time)
      runtime += 2000; // Add two seconds so it displays a friendly time initially.
    } else {
      setHold();
    }

  }
}

void setHold() {
// Function to set Hold mode (or if already in hold, off)
  if (holdmode) {
    // If in hold mode, then transition to off
    setOff();
  } else {
    runtime = 0; 
    start_time = 0;
    holdmode = true; // Transition to hold mode
    #ifdef mqttenabled
    client.publish(mqttPresetStateTopic, "hold");
    #endif
  }
}

void setOff() {
  start_time = 0;
  runtime = 0;
  state = 0;
  holdmode = false; 
  digitalWrite(GPIO_HIGHrelay, HIGH);  // Turn OFF HIGH relay
  digitalWrite(GPIO_LOWrelay, HIGH);  // Turn OFF LOW relay
  display.clearDisplay();
  display.display();
  #ifdef mqttenabled
  client.publish(mqttStateTopic, "OFF");
  client.publish(mqttSpeedStateTopic, "off");
  client.publish(mqttPresetStateTopic, "off");
  client.publish(mqttPercentageStateTopic, "0");
  #endif
}

// ****** HTTP Server Functions ********

void handleRoot() {
  String response ="<!DOCTYPE html>"
  "<html>"
    "<head>";
    response += favicon;
    response += "<style>"
        ".button {"
          "background-color: #036bfc;"
          "border: none;"
          "color: white;"
          "padding: 7px 15px;"
          "text-align: center;"
          "cursor: pointer;"
        "}"
        " body {"
          "font-family:Arial, Helvetica, sans-serif;"
        "}"
      "</style>"
    "</head>"

    "<body>"
      "<center>"
        "<H1>Smart Whole House Fan</H1><br>"
        "<div id=\"refreshdata\"></div>"
        "<input type=\"button\" class=\"button\" onclick=\"location.href='/addtime1';\" value=\"Add 1 Hour\"> <br><br>"
        "<input type=\"button\" class=\"button\" onclick=\"location.href='/addtime2';\" value=\"Add 2 Hours\"> <br><br>"
        "<input type=\"button\" class=\"button\" onclick=\"location.href='/holdmode';\" value=\"Set Hold Mode\"> <br><br>"
        "<input type=\"button\" class=\"button\" onclick=\"location.href='/changespeed';\" value=\"High - Low - Off\"> <br><br>"
        "<input type=\"button\" class=\"button\" onclick=\"location.href='/diag';\" value=\"Diagnostics\"> <br><br>"
        "</center>"
    "</body>"
    "<script>"

    "function getData() {"
      "var xhr = new XMLHttpRequest();"   // Create XMLHttpRequest object

      "xhr.onload = function() {"                       // When response has loaded
      // The following conditional check will not work locally - only on a server
        "if(xhr.status === 200) {"                       // If server status was ok
          "document.getElementById('refreshdata').innerHTML = xhr.responseText;" // Update
        "}"
      "};"

      "xhr.open('GET', '/refreshdata', true);" // Prepare the request
      "xhr.send(null);"                        // Send the request
    "}"

    "window.onload = getData();"
    "setInterval(getData, 1000);"
    "</script>"
  "</html>";

  server.send(200, "text/html", response);   // Send HTTP status 200 (Ok) and send some text to the browser/client
}

void handleRefreshData() {
  String response = "";

  if(lockmode) {
    response += "<H4> LOCK Mode Enabled </H4><br><br>";
  }
  if(holdmode) {
    response += "<H4> HOLD Mode Enabled </H4><br><br>";
  } else if((start_time > 0) && ((millis() - start_time)) < runtime) {
    timestring = "";
    if ((runtime - (millis() - start_time)) < (60 * 60 * 1000)) {
      timestring += ((runtime - (millis() - start_time)) / 60000);
      timestring += "m ";
      timestring += ((runtime - (millis() - start_time)) / 1000) % 60;
      timestring += "s";
    } else {
      timestring += ((runtime - (millis() - start_time)) % (1000 * 60 * 60 * 24)) / (1000 * 60 * 60);  // Number of hours remaining
      timestring += "h ";
      timestring += ((runtime - (millis() - start_time)) % (1000 * 60 * 60)) / (1000 * 60); // Number of minutes remaining
      timestring += "m ";
    }
    response += "<H4>Time Remaining: ";
    response += timestring;
    response += "</H4>";
  }
  
  response += "<H4>Speed: <i>";
  // Display current state (High/Low/Off) in button
  if(state == 1) {
    response += "High";
  }
  else if (state == 2 ) {
    response += "Low";
  } else {
    response += "Off";
  }
  response += "</i></H4><BR><BR>";
  server.send(200, "text/html", response);

}

void handleDiag() {
  #ifdef mqttenabled
  String mqttstatus;
  if (!client.connected()) {
    mqttstatus = "<i style=\"color:red;\">MQTT is NOT connected.</i>";
  } else {
    mqttstatus = "<i style=\"color:green;\">MQTT is connected.</i>";
  }
  #endif 

  String SSID = WiFi.SSID();
  String localIP = IpAddress2String(WiFi.localIP());

  String response ="<!DOCTYPE html>"
  "<html>"
    "<head>";
    response += favicon;
    response += "<style>"
        ".button {"
          "background-color: #036bfc;"
          "border: none;"
          "color: white;"
          "padding: 7px 15px;"
          "text-align: center;"
          "cursor: pointer;"
        "}"
        " body {"
          "font-family:Arial, Helvetica, sans-serif;"
        "}"
      "</style>"
    "</head>"

    "<body>"
      "<center>"
        "<H1>Diagnostics</H1><br>"
        "<div id=\"refreshdata\"></div>"
        "<div><b>SSID: </b>";
  response += SSID;
  response += "<br><b>IP Address: </b>";
  response += localIP;
  #ifdef mqttenabled
  response += "<br><b>MQTT Status: </b>";
  response += mqttstatus;
  #endif
  response += "<br></div><br>"
        "<input type=\"button\" class=\"button\" onclick=\"location.href='/restart';\" value=\"RESTART\"> <br><br>"
        "<input type=\"button\" class=\"button\" onclick=\"location.href='/togglelock';\" value=\"Toggle Lock\"> <br><br>"
        "<input type=\"button\" class=\"button\" onclick=\"location.href='/';\" value=\"Home\"> <br><br>"
        "</center>"
    "</body>"
    "<script>"

    "function getData() {"
      "var xhr = new XMLHttpRequest();"   // Create XMLHttpRequest object

      "xhr.onload = function() {"                       // When response has loaded
      // The following conditional check will not work locally - only on a server
        "if(xhr.status === 200) {"                       // If server status was ok
          "document.getElementById('refreshdata').innerHTML = xhr.responseText;" // Update
        "}"
      "};"

      "xhr.open('GET', '/refreshdata', true);" // Prepare the request
      "xhr.send(null);"                        // Send the request
    "}"

    "window.onload = getData();"
    "setInterval(getData, 1000);"
    "</script>"
  "</html>";

  server.send(200, "text/html", response);   // Send HTTP status 200 (Ok) and send some text to the browser/client
}

void handleRestart() {
  Serial.println("Restart Requested.");
  server.sendHeader("Location", String("/"), true);  // Redirect so you don't get into an endless loop of restarts.
  server.send(302, "text/plain", "");
  display.clearDisplay();
  display.setFont();
  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);
  display.println("Restarting...");
  display.display();
  delay(2000);
  ESP.restart();
}

void handleToggleLock() {
  lockmode = !lockmode;
  server.sendHeader("Location", String("/diag"), true); 
  server.send(302, "text/plain", "");
}

void handleAPI() {
  StaticJsonDocument<128> responsedoc;

  if (server.hasArg("plain")== true){ // Check if body received
    // If POSTed data, do actions
    // Get json data 
    String message = server.arg("plain");
    StaticJsonDocument<128> requestdoc;
    DeserializationError error = deserializeJson(requestdoc, message);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      server.send(200, "text/plain", error.f_str());
      return;
    }
    
    // API Request Structure
    // {
    //  "power" : "on", "off"           # Turn on or off
    //  "speed" : "high", "low", "off"  # Can be high, low or off
    //  "holdmode" : True, False        # Can be True or False
    //  "addtime" : 1,2                 # Add hours can be 1 or 2
    // }

    if (requestdoc["power"] == "on\0") {
      state = 1;
      setSpeed();
    } else if (requestdoc["power"] == "off\0") {
      setOff();
    }

    if (requestdoc["speed"] == "high\0") {
      state = 1;
      setSpeed();
    } else if (requestdoc["speed"] == "low\0") {
      state = 2;
      setSpeed();
    } else if (requestdoc["speed"] == "off\0") {
      setOff();
    }

    if (requestdoc["holdmode"]) {
      holdmode = true;
      setTimestring();
    } else {
      holdmode = false;
      setTimestring();
    }

    if (requestdoc["addtime"] == 1) {
      addTime(1);
      setTimestring();
    } else if (requestdoc["addtime"] == 2) {
      addTime(2);
      setTimestring();
    }
  }

  // Send back state structure
  if(state == 0) {
    responsedoc["power"] = "off";
    responsedoc["speed"] = "off";
  } else if (state == 1) {
    responsedoc["power"] = "on";
    responsedoc["speed"] = "high";
  } else if (state == 2) {
    responsedoc["power"] = "on";
    responsedoc["speed"] = "low";
  } else {
    responsedoc["power"] = "on";
    responsedoc["speed"] = "unknown";
  }

  if(holdmode) {
    responsedoc["holdmode"] = true;
    responsedoc["timer"] = "holdmode"; 
  } else if((start_time > 0) && ((millis() - start_time)) < runtime) {
    responsedoc["holdmode"] = false;
    responsedoc["timer"] = timestring; 
  }  

  char output[128];

  serializeJson(responsedoc, output);

  server.send(200, "text/plain", output);
  Serial.print("API Request: ");
  Serial.println(output);
}

void handleAddTime1() {
  
  addTime(1);

  server.sendHeader("Location", String("/"), true);
  server.send ( 302, "text/plain", "");
}

void handleAddTime2() {

  addTime(2);

  server.sendHeader("Location", String("/"), true);
  server.send ( 302, "text/plain", "");
}

void handleHoldMode() {
    
  setHold();

  server.sendHeader("Location", String("/"), true);
  server.send ( 302, "text/plain", "");
}

void handleChangeSpeed() {
  state += 1;
  
  setSpeed();
  
  server.sendHeader("Location", String("/"), true);
  server.send ( 302, "text/plain", "");
}

void handleNotFound(){
  server.sendHeader("Location", String("/"), true);
  server.send ( 302, "text/plain", "");  // Redirect to home
  //server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}


#ifdef mqttenabled
// ****** MQTT Functions ********

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] [");

  // print payload

  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  Serial.print(message);
  Serial.println("]");
  
  // Command Topic Parsing
  if (strcmp(topic,mqttCommandTopic)==0) {
    Serial.println("Command Topic Selected.");
    if (strcmp(message,"ON")==0) {
      Serial.println(" - Turning ON ");
      state = 1;
      setSpeed();
      addTime(1);
    }
    if (strcmp(message,"OFF")==0) {
      Serial.println(" - Turning OFF ");
      setOff();
    }
  }
  
  // Preset Command Topic Parsing
  if (strcmp(topic,mqttPresetCommandTopic)==0) {
    Serial.println("Preset Command Topic Selected.");
    if (strcmp(message,"off")==0) {
      Serial.println(" - Turning OFF ");
      setOff();
    }
    if (strcmp(message,"hold")==0) {
      Serial.println(" - Toggling Holdmode ");
      setHold();
    }
    if (strcmp(message,"timer")==0) {
      Serial.println(" - Timer Mode ");
      holdmode = false;
      addTime(1);
    }
  }

  // Speed Command Topic Parsing
  if (strcmp(topic,mqttSpeedCommandTopic)==0) {
    Serial.println("Speed Command Topic Selected.");
    if (strcmp(message,"off")==0) {
      Serial.println(" - Turning OFF ");
      setOff();
    }
    if (strcmp(message,"high")==0) {
      Serial.println(" - setting high ");
      state = 1;
      setSpeed();
    }
    if (strcmp(message,"low")==0) {
      Serial.println(" - setting low ");
      state = 2;
      setSpeed();
    }
  }

  // Speed Command Topic Parsing
  if (strcmp(topic,mqttPercentageCommandTopic)==0) {
    Serial.println("Percentage Command Topic Selected.");
    if (strcmp(message,"0")==0) {
      Serial.println(" - Turning OFF ");
      setOff();
    }
    if (strcmp(message,"2")==0) {
      Serial.println(" - setting high ");
      state = 1;
      setSpeed();
    }
    if (strcmp(message,"1")==0) {
      Serial.println(" - setting low ");
      state = 2;
      setSpeed();
    }
  }

}

void reconnect() {
  Serial.print("Attempting MQTT connection...");
  String event;

  if (client.connect("smartwhfclient", mqttUser.c_str(), mqttPassword.c_str() )) {
    event = "MQTT Connected.";
  } else {
    Serial.print("failed with state ");
    Serial.print(client.state());
    event = "MQTT Failed.";
  }

  Serial.println(event);

  if(client.connected()) {
    client.subscribe(mqttCommandTopic);
    client.subscribe(mqttPresetCommandTopic);
    client.subscribe(mqttSpeedCommandTopic);
    client.subscribe(mqttAvailTopic);

    // Publish initial state (TBD)
    if(state > 0) {
      client.publish(mqttStateTopic, "ON");
      if(state == 1) {
        client.publish(mqttSpeedStateTopic, "high");
      } else if (state == 2) {
        client.publish(mqttSpeedStateTopic, "low");
      } else {
        client.publish(mqttPresetStateTopic, "off");
      }
    } else {
      client.publish(mqttStateTopic, "OFF");
      client.publish(mqttSpeedStateTopic, "off");
      client.publish(mqttPresetStateTopic, "off");
    }
  }

}

#endif