// IMPORTANT! The BLYNK_TEMPLATE_ID and BLYNK_DEVICE_NAME must be updated by user and uploaded onto the NodeMCU via USB cable before use of this product!
#define BLYNK_TEMPLATE_ID "xxx"
#define BLYNK_DEVICE_NAME "Doorbell"

#define BLYNK_FIRMWARE_VERSION "0.1.0"

#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG

#define APP_DEBUG

#include "BlynkEdgent.h"
// Uncomment your board, or configure a custom board in Settings.h
//#define USE_SPARKFUN_BLYNK_BOARD
#define USE_NODE_MCU_BOARD
//#define USE_WITTY_CLOUD_BOARD

#define LED D4            // LED in NodeMCUv3 at pin GPIO2 (D4).
#define Bell_Trigger D6   // Doorbell connected at pin GPIO12 (D6).

unsigned long last_changed = 0;
unsigned long last_triggered = 0;
unsigned long last_flickered = 0;
bool gradient = 0;  // 1 means increasing LED intensity; 0 means decreasing LED intensity
byte dutyCycleStep = 255; // 256 steps of LED brightness ranging from 0 to 255
byte dutyCycle = 255; // function-mapped steps



/*
BLYNK_WRITE(V0)
{
  int pin=param.asInt();
  digitalWrite(2,pin);
  }

BLYNK_WRITE(V1)
{
  int pin=param.asInt();
  digitalWrite(4,pin);
  }
 */

void setup()
{
  Serial.begin(115200);
  delay(100);
  
  pinMode(LED, OUTPUT);    // LED pin as output.
  pinMode(Bell_Trigger, INPUT_PULLUP); // Bell_Trigger pin as input. Pull up so that voltage does not float arbitrarily after testing the trigger by shorting it to ground.

  BlynkEdgent.begin();
}

void loop() {
  BlynkEdgent.run();

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
  Serial.println("Someone's at the door!"); // For debugging purpose only to test the trigger when the Doorbell is connected to your PC to listen to the Serial Monitor.
  Blynk.logEvent("knock_knock", "Someone's at the door!"); // Send a notification to your mobile device. Note that further configuration in Blynk.Console is required.

}
