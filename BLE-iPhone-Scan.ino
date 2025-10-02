#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <time.h>
//#include <sys/time.h>

const int RED_LED = D9;
const int scanTime = 3; // seconds
const int lightSensorPin = A0;  // Use the correct analog pin for your XIAO ESP32
const int deviceDistanceThreshold = 61;
//const int deviceCountThreshold = 4;
const int lightSensorThreshold = 2000;
const float deviceCountChangeThreshold = 0.6;
int historyArrayIndex = 0;
int historyCount = 0;
const int numHistoryValues = 4;
int deviceNearbyCountHistory[numHistoryValues];
//const unsigned long countdownTimeMinutes = 5;
const unsigned long countdownTimeSeconds = 10; //countdownTimeMinutes * 60;
bool timeInputReceived = false;
bool darkEnough = false;
bool timeOfDayAllowed = false;
bool startTimerCountdown = false;
unsigned long startTimerTime = 0;
BLEScan* pBLEScan;

void setup() {
  Serial.begin(115200);

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();  // create scan object
  pBLEScan->setActiveScan(true);    // active scan for more data

  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  while (((millis() / 1000) < 3) & (!Serial)) {   // Wait for Serial Monitor to open
    digitalWrite(LED_BUILTIN, LOW); // turn the LED on
    digitalWrite(RED_LED, HIGH);    // turn the Red LED on
    delay(100);                     // wait before LED off
    digitalWrite(LED_BUILTIN, HIGH);// turn the LED off
    digitalWrite(RED_LED, LOW);     // turn the Red LED off
    delay(100);                     // wait before LED on
  }  
  //delay(2000);

  Serial.println("\n\n\n\n\n\n\n\n\n\n\nStarting BLE scan...\n");
  Serial.println("Enter time as HH:MM (24-hour format)");
  Serial.println("Example: 22:30 for 10:30 PM\n");

  String input = "";
  while ((input.length() == 0) & ((millis() / 1000) < 15)) {  // wait a maximum of 15 seconds for time input
    if (Serial.available()) {
      input = Serial.readStringUntil('\n');
      input.trim(); // Remove any newline/whitespace
      timeInputReceived = true;
    } else {
      digitalWrite(LED_BUILTIN, LOW); // turn the LED on
      delay(50);                      // wait before LED off
      digitalWrite(LED_BUILTIN, HIGH);// turn the LED off
      delay(100);                     // wait before LED on
    }
  }

  // Parse hour and minute
  int hour = 19;    // defaut time set to 7:00pm in case there's no input
  int minute = 0;
  if (timeInputReceived) {
    hour = input.substring(0, input.indexOf(':')).toInt();    // read number before ":"
    minute = input.substring(input.indexOf(':') + 1).toInt(); // read number after ":"
  }

  // Set default date (doesn't matter unless you care about the calendar)
  struct tm t;
  t.tm_year = 2025 - 1900;  // Year
  t.tm_mon  = 0;            // January (0-based)
  t.tm_mday = 1;            // 1st
  t.tm_hour = hour;
  t.tm_min  = minute;
  t.tm_sec  = 0;
  t.tm_isdst = 0;

  time_t timeSinceEpoch = mktime(&t);
  struct timeval now = { .tv_sec = timeSinceEpoch };
  settimeofday(&now, NULL);

  //Serial.println("Time set to 10:00 AM");
  Serial.print("\nTime set to ");
  Serial.print(hour);
  Serial.print(":");
  if (minute < 10) {
    Serial.print("0");  // Add leading zero for minutes less than 10
  }
  Serial.print(minute);
  if (hour < 13) {
    Serial.println("AM\n");
  } else {
    Serial.println("PM\n");
  }

  // initialize the historical values array
  for (int i = 0; i < numHistoryValues; i++) {
    deviceNearbyCountHistory[i] = 0;
    i = (i + 1) % numHistoryValues;
  }
}

void loop() {
  int deviceTotalFoundCount = 0;
  int deviceNearbyCount = 0;
  float averageNumDevices = 0;
  float avgSum = 0;

  struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to get time");
      return;
    }

  if (1 | (!startTimerCountdown) | (historyCount < numHistoryValues)) {
    BLEScanResults foundDevices = *pBLEScan->start(scanTime, false);
    Serial.println("\n\n\n------------------------------------------------\n\nScan complete!");

    for (int i = 0; i < foundDevices.getCount(); i++) {
      BLEAdvertisedDevice device = foundDevices.getDevice(i);
      String mfgData = device.getManufacturerData();

      // Check if the manufacturer ID corresponds to Apple (0x004C)
      if (mfgData.length() >= 2) {
        const uint8_t* payload = (const uint8_t*)mfgData.c_str();
        
        // Apple Manufacturer ID is 0x004C (check first 2 bytes)
        if (payload[0] == 0x4C && payload[1] == 0x00) {
          deviceTotalFoundCount++;
          //Serial.print("\n📶 Apple device located!");
          
          if (device.getRSSI() > -deviceDistanceThreshold) {
            deviceNearbyCount++;
            // Serial.print("\n- MAC:\t");
            // Serial.println(device.getAddress().toString().c_str());
            Serial.print("- RSSI:\t");
            Serial.print(device.getRSSI());
            Serial.println("\t📶 Apple device is nearby!");
          }
          //Serial.println(" ");
        }
      }
    }
  } else {
    delay(150);
    Serial.println("\n\n\n------------------------------------------------\n\nScan skipped!");
  }

  if (!startTimerCountdown | (historyCount < numHistoryValues)) {
    deviceNearbyCountHistory[historyArrayIndex] = deviceNearbyCount;
    historyArrayIndex = (historyArrayIndex + 1) % numHistoryValues;
  }
  if (historyCount < numHistoryValues) {
    historyCount++;
  }

  for (int i = 0; i < numHistoryValues; i++) {
    avgSum = avgSum + deviceNearbyCountHistory[i];
    averageNumDevices = avgSum / numHistoryValues;
  }

  ////Outputs Below!

  if (startTimerCountdown) {
    unsigned long timeElapsed = (millis() / 1000) - startTimerTime;
    if ((timeElapsed) >= countdownTimeSeconds) {
      Serial.println("\n\n\n\n\n\nDisable Lightbulb!\n\n\n\n\n");
      startTimerCountdown = false;

      //Disable LED
    //digitalWrite(LED_BUILTIN, LOW);  // turn the LED off
    //digitalWrite(RED_LED, HIGH);  // turn the Red LED off
    } else {
      Serial.print("\nThe lightbulb on timer has ");
      Serial.print((countdownTimeSeconds - timeElapsed));
      Serial.println(" seconds remaining.");
    }
  }

  // if ((!startTimerCountdown) & (deviceNearbyCount >= deviceCountThreshold)) {
  //   startTimerCountdown = true;
  //   startTimerTime = (millis() / 1000);
  //   Serial.print("A timer has begun for ");
  //   Serial.print(countdownTimeSeconds);
  //   Serial.println(" seconds!");
  // }

  if ((!startTimerCountdown) & (deviceNearbyCount >= (deviceCountChangeThreshold + averageNumDevices))) {
    startTimerCountdown = true;
    startTimerTime = (millis() / 1000);
    Serial.print("A timer has begun for ");
    Serial.print(countdownTimeSeconds);
    Serial.println(" seconds!");
  }

  int sensorValue = analogRead(lightSensorPin);  // Range: 0 to 4095 on ESP32
  Serial.print("Light Level: ");
  Serial.println(sensorValue);
  if (sensorValue < lightSensorThreshold) {
    darkEnough = true;
  } else {
    darkEnough = false;
  }

  // Serial.print(deviceTotalFoundCount);
  // Serial.println(" Apple devices found!");

  Serial.print(deviceNearbyCount);
  Serial.println(" Apple device(s) are nearby!");

  Serial.print(averageNumDevices);
  Serial.println(" Apple device(s) nearby recently!");

  for (int i = 0; i < numHistoryValues; i++) {
    Serial.print(deviceNearbyCountHistory[i]);
    Serial.print(" ");
  }
  Serial.print("  - ");
  Serial.println(historyCount);

  //toggle time of day
  Serial.print(&timeinfo, "Current time: %H:%M:%S");
  if (((timeinfo.tm_hour > 4) & (timeinfo.tm_hour < 11)) | ((timeinfo.tm_hour > 18) | (timeinfo.tm_hour < 2))){
    timeOfDayAllowed = true;
    Serial.println("\tNight time toggled true!");
  } else {
    Serial.println(" ");
  }

  if(startTimerCountdown & darkEnough & timeOfDayAllowed) {
    digitalWrite(LED_BUILTIN, LOW);  // turn the LED on
    digitalWrite(RED_LED, HIGH);  // turn the Red LED on
  } else {
    for (int i = 0; i < deviceNearbyCount; i++) {
      digitalWrite(LED_BUILTIN, LOW);  // turn the LED on
      digitalWrite(RED_LED, HIGH);  // turn the Red LED on
      delay(150); // wait before LED off
      digitalWrite(LED_BUILTIN, HIGH);  // turn the LED off
      digitalWrite(RED_LED, LOW);  // turn the Red LED off
      delay(150); // wait before LED off
    }
  }

  pBLEScan->clearResults(); // clear buffer
  delay(100); // wait before next scan
  // digitalWrite(LED_BUILTIN, HIGH);  // turn the LED off
  // digitalWrite(RED_LED, LOW);  // turn the Red LED off
  timeOfDayAllowed = false;
}
