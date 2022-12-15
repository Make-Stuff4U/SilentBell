/*********
  Credits to Rui Santos for the base code of WifiManager with Custom Parameters
  Complete project details at http://randomnerdtutorials.com  
*********/

/* A NOTE FROM MAKE STUFF
PRECAUTION:
To avoid power supply collision, do not connect the Doorbell to your PC via the USB port when the Doorbell is powered on via the mains.
This is only for precaution as there is a very remote chance of hardware damage either to your PC or to the NodeMCU should there be a power supply collision.

LIMITATION:
The mains circuit receiving the radio signal and the low voltage NodeMCU circuit are electrically isolated.
When the Doorbell is powered via the mains, both the radio receiver and the NodeMCU circuit are powered up separately.
However, when the Doorbell is powered via your PC's USB port, the radio signal receiver does not get any power.
This means that it is not possible to debug via the Serial Monitor any part of the code that relies on the activation of the radio receiver.  

HOW TO RESET WIFI CREDENTIALS AND ADDITIONAL PARAMETERS:
You can only enter WiFi credentials and the 64-character array webhooks_key as well as the 32-character array maker_event once.
Thereafter, upon successful WiFi connection, the AP is no longer available.
To reset these values, first use your Arduino studio to erase all flash contents of your NodeMCU and reload the code. This will put the NodeMCU back in AP mode.

END OF NOTE FROM MAKE STUFF */

#include <FS.h> //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson
#include "AnotherIFTTTWebhook.h"

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

#define LED D4            // LED in NodeMCUv3 at pin GPIO2 (D4).
#define Bell_Trigger D6   // Doorbell connected at pin GPIO12 (D6).

unsigned long last_changed = 0;
unsigned long last_triggered = 0;
unsigned long last_flickered = 0;
bool gradient = 0;  // 1 means increasing LED intensity; 0 means decreasing LED intensity
byte dutyCycleStep = 255; // 256 steps of LED brightness ranging from 0 to 255
byte dutyCycle = 255; // function-mapped steps

// Auxiliar variables to store the current output state
String outputState = "off";

// Assign output variables to GPIO pins
//char output[2];
char webhooks_key[64];
char maker_event[32];

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup() {
  
  Serial.begin(115200);
  
  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          //strcpy(output, json["output"]);
          strcpy(webhooks_key, json["webhooks_key"]);
          strcpy(maker_event, json["maker_event"]);
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
  
  //WiFiManagerParameter custom_output("output", "output", output, 2);
  WiFiManagerParameter custom_webhooks_key("key", "Webhooks Key", webhooks_key, 64);
  WiFiManagerParameter custom_maker_event("event", "Maker Event", maker_event, 32);

  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  // set custom ip for portal
  //wifiManager.setAPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //add all your parameters here
  //wifiManager.addParameter(&custom_output);
  wifiManager.addParameter(&custom_webhooks_key);
  wifiManager.addParameter(&custom_maker_event);

  
  // Uncomment and run it once, if you want to erase all the stored information
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //if WiFi credentials already exist, then sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  if (wifiManager.getWiFiIsSaved()) wifiManager.setTimeout(180);

  // fetches ssid and pass from eeprom and tries to connect
  // if it does not connect it starts an access point with the specified name
  // here  "AutoConnectAP"
  // and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("AutoConnectAP");
  // or use this for auto generated name ESP + ChipID
  //wifiManager.autoConnect();
  
  // if you get here you have connected to the WiFi
  Serial.println("Connected.");
  
  //strcpy(output, custom_output.getValue());
  strcpy(webhooks_key, custom_webhooks_key.getValue());
  strcpy(maker_event, custom_maker_event.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    //json["output"] = output;
    json["webhooks_key"] = webhooks_key;
    json["maker_event"] = maker_event;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  /*
  // Initialize the output variables as outputs
  pinMode(atoi(output), OUTPUT);
  // Set outputs to LOW
  digitalWrite(atoi(output), LOW);;
  */

  pinMode(LED, OUTPUT);    // LED pin as output.
  pinMode(Bell_Trigger, INPUT_PULLUP); // Bell_Trigger pin as input. Pullup also needs to be specified otherwise it wont work with a floating push switch.
  
  server.begin();
}

void loop(){
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // turns the GPIOs on and off
            if (header.indexOf("GET /output/on") >= 0) {
              Serial.println("Output on");
              outputState = "on";
              //digitalWrite(atoi(output), HIGH);
            } else if (header.indexOf("GET /output/off") >= 0) {
              Serial.println("Output off");
              outputState = "off";
              //digitalWrite(atoi(output), LOW);
            }
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #77878A;}</style></head>");
            
            // Web Page Heading
            client.println("<body><h1>ESP8266 Web Server</h1>");
            
            // Display current state, and ON/OFF buttons for the defined GPIO  
            client.println("<p>Output - State " + outputState + "</p>");
            // If the outputState is off, it displays the ON button       
            if (outputState=="off") {
              client.println("<p><a href=\"/output/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/output/off\"><button class=\"button button2\">OFF</button></a></p>");
            }                  
            client.println("</body></html>");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }

  if ((!digitalRead(Bell_Trigger) == true) & (millis()-last_triggered > 5500)) {
    //digitalWrite(LED, LOW); // Turn the LED on for 5.5 seconds only. This is around the duration of the default Daiyo Doorbell ringtone. If it is too short, it will lead to multiple notifications per press.
    //Serial.println("Turn the LED ON by making the voltage at D4 LOW"); // For debugging purpose only to test the trigger when the Doorbell is connected to your PC to listen to the Serial Monitor.
    do_something(); // Call your function when triggered.
    last_triggered = millis();
  } else
  
  {
    if (millis()-last_triggered > 5500) {
      blinking(); // EITHER: Blink the LED to indicate that the Doorbell is actively waiting for the press of the Doorbell.
    } else {
      flickering(); // OR: Flicker the LED to indicate that the Doorbell has just been triggered in the last 5.5 seconds.
    }
  }
}

void flickering() {
  if (millis()-last_flickered > 200) {
    digitalWrite(LED, !digitalRead(LED)); // Invert every 200ms
    last_flickered = millis();
  }
}

void blinking() {
  if (millis()-last_changed > 5) {
    analogWrite(LED, dutyCycleStep);
    // Serial.println("The PWM duty cycle of the LED has been updated at D4");
    last_changed = millis();

    //Sawtooth oscillation of dutyCycleStep
    if (gradient == 0) {
      dutyCycleStep--;
      if (dutyCycleStep == 0) {gradient = !gradient;}
    } else {
      dutyCycleStep++;
      if (dutyCycleStep == 255) {gradient = !gradient;}
    }
    
    dutyCycle = (255/8)*pow(2,(((float)dutyCycleStep/255)*3)); // Exponential growth and fade. Need to cast into float otherwise will get integer result for intermediate calculations.

  }
}

void do_something () {
  // USER'S CODE GOES HERE TO DO SOMETHING WHEN THE DOORBELL IS PRESSED.
  // You can make use of the user-provided parameters, the 64-character array webhooks_key as well as the 32-character array maker_event, which were entered by user from the WiFi configuration page and saved into the memory of the NodeMCU.
  Serial.println("Your Webhooks Key has been saved as:");
  Serial.println(webhooks_key);
  Serial.println("Your Maker Event has been saved as:");
  Serial.println(maker_event);

  // Send Webook to IFTTT
  send_webhook(maker_event, webhooks_key, "", "", "");

}
