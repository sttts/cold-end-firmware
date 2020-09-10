/*

  ColdEND v1.0 Minimum Quantity Lubrication
  https://www.end-cnc-shop.de/geloetetes/3/pumpen-steuerung-1.5-bauteile-set

  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License

  Based on firmware by:
  Sebastian End / END-CNC
  Daniel Seoane / SeoGeo

  Rewritten by Tilman, 2020-09-07

*/


#define potMist A0
#define potSpit A1

#define SSD1306                             // Uncomment for OLED with SSD1306 controller
// #define SH1106                              // Uncomment for OLED with SH1106 controller

#define SCREEN_WIDTH 128                    // OLED display width, in pixels
#define SCREEN_HEIGHT 32                    // OLED display height, in pixels
#define OLED_RESET     4                    // Reset pin # (or -1 if sharing Arduino reset pin)


// to be calibrated
float mist_max_flow_rate = 50;              // Maximum coolant flow step delay (min. delay time between step LOW and HIGH; calibrate to match ml_per_hour)
int ml_per_hour = 150;                      // Maximum milliliter per hour (needs to be metered before)
int ml_per_hour_min = 1;                    // Minimum milliliter per hours


// #define LINEAR_MILLILITER_PER_SECOND     // Set to 1 for linear control of ml/s
#define EXPONENTIAL_MILLILITER_PER_SECOND   // Set to 0 for exponential control of ml/s


#ifdef SSD1306
  #define oled
  #include <Wire.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
  #include <Fonts/FreeSans18pt7b.h>
  #include <Fonts/FreeSans12pt7b.h>
  Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#endif

#ifdef SH1106
  #define oled
  #include <Wire.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_SH1106.h>
  #include <Fonts/FreeSans18pt7b.h>
  #include <Fonts/FreeSans12pt7b.h>
  Adafruit_SH1106 display(OLED_RESET);
#endif


// Pin definition
const int outDir = 2;                       // Direction pin
const int outStep = 3;                      // Step pin
const int outEna = 4;                       // Enable pin
const int inFast = 7;                       // Fast switch
const int inMist = 8;                       // Mist switch
const int inAir = 9;                        // Air switch
const int outAirValve = 10;                 // Air valve pin
const int outMistValve = 11;                // Mist valve pin
const int outSpitLED = 12;                  // Spit LED pin


// Mist and spit variables
float mist_flow_rate;                       // Coolant flow rate value
float fast_flow_rate = 100;                 // Fast mode flow rate value
float spit_flow_rate = 100;                 // Spit flow rate value
float spit_time;                            // Spit mode time
float spit_min_time = 1000;                 // Spit mode minimum time in milliseconds
float spit_max_time = 8000;                 // Spit mode maximum time in milliseconds
int spit_stat = 0;                          // Spit mode state
int mist_pot;                               // Raw mist potentiometer value
int mist_prev;                              // Previous smoothed mist potentiometer value
int mist_smooth;                            // Smoothed mist potentiometer value
int spit_pot;                               // Raw spit potentiometer value
int air_valve = 0;                          // Air valve state (on/off)
int mist_valve = 0;                         // Mist valve state (on/off)
unsigned long spit_start;                   // Spit mode start time in millis
unsigned long spit_stop;                    // Spit mode stop time in millis


// Display variables
float mist_pot_val;
float mist_pot_old;
int spit_pot_val;
int spit_pot_old;
int mist_valve_old;
int air_valve_old;
int spit_mode;


void setup() {
  // Set output pins
  pinMode(outStep, OUTPUT);
  pinMode(outDir, OUTPUT);
  pinMode(outEna, OUTPUT);
  pinMode(outMistValve, OUTPUT);
  pinMode(outAirValve, OUTPUT);
  pinMode(outSpitLED, OUTPUT);

  // Set input pins
  pinMode(inMist, INPUT_PULLUP);
  pinMode(inFast, INPUT_PULLUP);
  pinMode(inAir, INPUT_PULLUP);

  // Set stepper pins
  digitalWrite(outDir, LOW);                // Change direction with HIGH and LOW
  digitalWrite(outEna, HIGH);               // Disable stepper

  // Initialize I2C OLED
  #ifdef SSD1306
    Serial.begin(9600);
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  #endif
  
  #ifdef SH1106
    Serial.begin(9600);
    display.begin(SH1106_SWITCHCAPVCC, 0x3C);
  #endif
}


void loop() {
  readSpitPot();                            // Read values from spit pot
  readMistPot();                            // Read values from mist pot
  
  if (spit_time > spit_min_time) {          // Activate spit LED when spit time > 1s
    digitalWrite(outSpitLED, HIGH);
  }
  else {
    digitalWrite(outSpitLED, LOW);
  }
  
  if (digitalRead(inAir) == LOW) {          // Manually switching air valve
    openAirValve();
  }
  else {
    closeAirValve();
  }

  if (digitalRead(inFast) == LOW) {         // Switch to fast mode (overrides normal operation)
    moveStepper(fast_flow_rate);
  }
  else if (digitalRead(inMist) == LOW) {    // Open air valve, check for spit mode and switch to normal operation
    openAirValve();
    spitMode();
    moveStepper(mist_flow_rate);
  }
  else {                                    // Stops normal operation (including spit mode)
    stopStepper();
    spit_stat = 0;
  }

  #ifdef oled
    prepareDisplay();                       // Refresh display if necessary
  #endif
}


void spitMode() {
  // Spit mode, check for conditions and run for a predefined time
  // If spit mode at times does not start, increase delay time to debounce switch
  if (spit_stat == 0 && spit_time > spit_min_time) {
    air_valve = 1;
    spit_mode = 1;
    
    #ifdef oled
      prepareDisplay();
    #endif
    
    delay(30);
    spit_start = millis();
    spit_stop = spit_start + spit_time;
    while ((digitalRead(inMist) == LOW) && (millis() < spit_stop)) {
      moveStepper(spit_flow_rate);
    }
    spit_mode = 0;
  }
  spit_stat = 1;
}

void delayMicrosecondsLong(long t) {
  while (t > 16383) {
    delayMicroseconds(16383);
    t -= 16383;
  }
  delayMicroseconds(t);
}

void moveStepper(int delay) {
  // Open mist valve and run stepper at desired speed
  digitalWrite(outMistValve, LOW);
  digitalWrite(outEna, LOW);
  digitalWrite(outStep, HIGH);
  delayMicrosecondsLong(delay);
  digitalWrite(outStep, LOW);
  delayMicrosecondsLong(delay);
  mist_valve = 1;
}


void stopStepper() {
  digitalWrite(outEna, HIGH);
  digitalWrite(outMistValve, HIGH);
  mist_valve = 0;
}


void openAirValve() {
  digitalWrite(outAirValve, LOW);
  air_valve = 1;
}


void closeAirValve() {
  digitalWrite(outAirValve, HIGH);
  air_valve = 0;
}

float exp_scale = log(ml_per_hour/ml_per_hour_min);

void readMistPot() {
  // Read mist pot, smooth the value and convert it for stepper speed and display output
  mist_prev = mist_smooth;
  mist_pot = analogRead(potMist);
  mist_smooth = 0.1 * mist_pot + 0.9 * mist_prev;
#if defined(LINEAR_MILLILITER_PER_SECOND)
  mist_pot_val = int(max(map(mist_smooth, 0, 1000, ml_per_hour, ml_per_hour_min), ml_per_hour_min));
#elif defined(EXPONENTIAL_MILLILITER_PER_SECOND)
  mist_pot_val = exp(max(0, 1000-mist_smooth)/1000.0*exp_scale)*ml_per_hour_min;
  if (mist_pot_val < 10) {
    mist_pot_val = int(mist_pot_val*10)/10.0;
  } else {
    mist_pot_val = int(mist_pot_val);
  }
#else
  #error LINEAR_MILLILITER_PER_SECOND or EXPONENTIAL_MILLILITER_PER_SECOND must be defined
#endif
  mist_flow_rate = mist_max_flow_rate * ml_per_hour / mist_pot_val;
}


void readSpitPot() {
  // Read spit pot and convert value for spit time and display output
  spit_pot = analogRead(potSpit);
  spit_pot_val = map(spit_pot, 0, 911, 8, 0);
  spit_time = map(spit_pot, 0, 1023, spit_max_time, 0);
}


#ifdef oled
void prepareDisplay() {
  // Check for new values by comparing them to previous values
  // Refresh OLED content only when any of the displayed values have changed
  if (mist_pot_val != mist_pot_old || spit_pot_val != spit_pot_old || mist_valve != mist_valve_old || air_valve != air_valve_old) {
    refreshDisplay();
    mist_pot_old = mist_pot_val;
    spit_pot_old = spit_pot_val;
    mist_valve_old = mist_valve;
    air_valve_old = air_valve;
  }
}

void refreshDisplay() {
  display.clearDisplay();
#if SCREEN_HEIGHT >= 64
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("Coolant");
  display.setCursor(101, 0);
  display.print("Spit");
  display.drawLine(0, 12, 128, 12, WHITE);
  display.setFont(&FreeSans18pt7b);
  display.setCursor(0, 42);
#ifdef EXPONENTIAL_MILLILITER_PER_SECOND
  if (mist_pot_val < 10) { 
    char s[4];
    dtostrf(mist_pot_val, 3, 1, s);
    display.print(s);
  } else {
    display.print(int(mist_pot_val));
  }
#else
  display.print(int(mist_pot_val));
#endif
  display.setFont();
  display.setTextSize(1);
  if (mist_pot_val < 10) {
#ifdef EXPONENTIAL_MILLILITER_PER_SECOND
    display.setCursor(51, 36);
#else
    display.setCursor(22, 36);
#endif
  }
  else if (mist_pot_val < 100) {
    display.setCursor(41, 36);
  }
  else {
    display.setCursor(60, 36);
  }
  display.print("ml/h");
  display.setFont(&FreeSans18pt7b);
  display.setCursor(101, 42);
  display.print(spit_pot_val);
  display.setFont();
  display.setTextSize(1);
  display.setCursor(122, 36);
  display.print("s");
  display.drawLine(0, 48, 128, 48, WHITE);
  display.setTextColor(BLACK);
  if (mist_valve == 1) {
    display.fillRect(0, 54, 63, 10, WHITE);
    display.setCursor(2, 55);
    display.print("Coolant On");
  }
  else if (spit_mode == 1) {
    display.fillRect(0, 54, 57, 10, WHITE);
    display.setCursor(2, 55);
    display.print("Spit Mode");
  }
  if (air_valve == 1) {
    display.fillRect(89, 54, 39, 10, WHITE);
    display.setCursor(91, 55);
    display.print("Air On");
  }
#else
  display.clearDisplay();
  display.setTextColor(WHITE);

  display.setCursor(101, 0);
  display.setFont(&FreeSans12pt7b);
  display.setCursor(0, 16);
#ifdef EXPONENTIAL_MILLILITER_PER_SECOND
  if (mist_pot_val < 10) {
    char s[4];
    dtostrf(mist_pot_val, 3, 1, s);
    display.print(s);
  } else {
    display.print(int(mist_pot_val));
  }
#else
  display.print(int(mist_pot_val));
#endif

  display.setFont();
  display.setTextSize(1);
  int x = 44;
  if (mist_pot_val < 10) {
#ifdef EXPONENTIAL_MILLILITER_PER_SECOND
    x = 37;
#else
    x = 18;
#endif
  } else if (mist_pot_val < 100) {
    x = 31;
  }
  display.setCursor(x, 0);
  display.print("Coolant");
  display.setCursor(x, 10);
  display.print("ml/h");

  display.setFont(&FreeSans12pt7b);
  display.setCursor(87, 16);
  display.print(spit_pot_val);
  display.setFont();
  display.setTextSize(1);
  display.setCursor(104, 0);
  display.print("Spit");
  display.setCursor(104, 10);
  display.print("s");

  display.setTextColor(BLACK);
  if (mist_valve == 1) {
    display.fillRect(0, 22, 63, 10, WHITE);
    display.setCursor(2, 23);
    display.print("Coolant On");
  }
  else if (spit_mode == 1) {
    display.fillRect(0, 22, 57, 10, WHITE);
    display.setCursor(2, 23);
    display.print("Spit Mode");
  }
  if (air_valve == 1) {
    display.fillRect(89, 22, 39, 10, WHITE);
    display.setCursor(91, 23);
    display.print("Air On");
  }
#endif
  display.display();
}
#endif
