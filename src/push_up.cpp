#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <avr/sleep.h>//this AVR library contains the methods that controls the sleep modes

#include "bitmaps.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const int buttonPin = 10;
int pushUpCount = 0;
int buttonState;
int lastButtonState = LOW;   // the previous reading from the input pin

// debounce for button
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

// delay for bitmap animation
unsigned long frame_interval = 300;
unsigned long frame_time_started = millis();

// timer for screen sleeping
unsigned long sleepInterval = 5000;
unsigned long sleepTimeStarted = millis();
bool isScreenSleeping = false;

// flag for checking which animation frame to draw
bool should_draw_up = true;

#define interruptPin 7 // setup for sleep

bool check_button_state() {
  bool isDifferent = false;
  int reading = digitalRead(buttonPin);

  // If the switch changed, due to noise or pressing:
  if (reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // if the button state has changed:
    if (reading != buttonState) {
      buttonState = reading;
      isDifferent = true;
    }
  }

  lastButtonState = reading;
  return isDifferent;
}

bool checkFrameTimer() {
  bool isUp = (millis() - frame_time_started >= frame_interval);
  if(isUp) {
    // Timer triggered - set new start time
    frame_time_started = millis();
  }
  return isUp;
}

void incrementCounter() {
  if(buttonState == HIGH) {
    pushUpCount += 1;
  }  
}

void drawPicture(const unsigned char bitmap [] PROGMEM){
  display.drawBitmap(30, -10, bitmap, 64, 64, WHITE);
}

void checkCurrentFrameAndDraw() {
  if(checkFrameTimer()) {
    display.fillRect(20, 0, 128, 32, BLACK);
    if(should_draw_up) {
      drawPicture(pushUpBitmap);
    } else {
        drawPicture(pushDownBitmap);
    }
    should_draw_up = !should_draw_up;
  }
}

// Clear and draw new text
void drawText(int text) {
  display.fillRect(0, 0, 16, 32, BLACK);
  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);             // Start at top-left corner
  display.println(text);
}

void drawScreen(void) {
  // Draw gif frame
  checkCurrentFrameAndDraw();

  // Draw pushup text
  drawText(pushUpCount);

  // Draw display
  display.display();
}

// Interrupt process, do not do anything other than disabling sleep here...
void wakeUp() {
  sleep_disable();
}

void sleep() {
  if(isScreenSleeping) {
    //return;
  }
  Serial.println("Going to sleep...");

  // disable the USB
  USBCON |= _BV(FRZCLK);  //freeze USB clock
  PLLCSR &= ~_BV(PLLE);   // turn off USB PLL
  USBCON &= ~_BV(USBE);   // disable USB

  isScreenSleeping = true;
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  attachInterrupt(digitalPinToInterrupt(interruptPin), wakeUp, LOW);
  sleep_mode();

  // we come here after wakeup
  detachInterrupt(digitalPinToInterrupt(interruptPin));
  display.ssd1306_command(SSD1306_DISPLAYON);

  delay(100);
  sei();
  USBDevice.attach(); // now re-open the serial port, hope that it is assigned same 'COMxx'
  delay(100);
  Serial.begin(9600);
  Serial.println("Just woke up!");
  isScreenSleeping = false;
}

bool checkScreenTimeout() {
  bool isUp = (millis() - sleepTimeStarted >= sleepInterval);
  if(isUp) {
    sleepTimeStarted = millis();
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    Serial.println("SLEEEEP");
    sleep();
  }
  return isUp;
}

void setup() {
  Serial.begin(9600);

  // SSD1305_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1305 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Set interrupt pin and assign test process
  pinMode(interruptPin, INPUT_PULLUP);

  // Clear the buffer
  display.clearDisplay();

  // Setup buttons
  pinMode(buttonPin, INPUT);

  // Initial display
  drawScreen();
}

void loop() {
  if(check_button_state()) {
    // Don't incremement if we were recently asleep
    if(!isScreenSleeping) {
      incrementCounter();
    }
  }
  
  drawScreen();
  checkScreenTimeout();
  Serial.println("Looping");
}
