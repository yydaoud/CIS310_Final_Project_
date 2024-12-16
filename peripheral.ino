/*
CIS 310 Final Project
Ciaran Grabowski, Hailey Gumbs, Yahya Daoud
12/15/2024
*/

#include <ArduinoBLE.h>
#include <Arduino_HS300x.h>
#include <Arduino_APDS9960.h>

#define VALUE_SIZE 20

#define SERVICE_UUID "00000000-5EC4-4083-81CD-A10B8D5CF6EC"
#define TEMP_CHARACTERISTIC_UUID "00000001-5EC4-4083-81CD-A10B8D5CF6EC"


int currColor = 0;
int colorOut = 0;


BLEService sensorService = BLEService(SERVICE_UUID);
BLECharacteristic temperatureCharacteristic = BLECharacteristic(TEMP_CHARACTERISTIC_UUID, BLERead | BLENotify, VALUE_SIZE);


// last temperature reading
int oldTemperature = 0;
// last time the temperature was checked in ms
long previousMillis = 0;

void setup() {
  // initialize serial communication
  Serial.begin(9600);
  while (!Serial)
    ;

  if (!HS300x.begin()) {
    Serial.println("Failed to initialize HS300x.");
    while (1)
      ;
  }
  Serial.println("Detecting Temperature...");

  if (!APDS.begin()) {
    Serial.println("Error initializing APDS9960 sensor!");
  }
  Serial.println("Detecting gestures ...");

  // initialize the built-in LED pin to indicate when a central is connected
  pinMode(LED_BUILTIN, OUTPUT);
  //in-built LED
  pinMode(LED_BUILTIN, OUTPUT);
  //Red
  pinMode(LEDR, OUTPUT);
  //Green
  pinMode(LEDG, OUTPUT);
  //Blue
  pinMode(LEDB, OUTPUT);


  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");

    while (1)
      ;
  }

  BLE.setLocalName("NANO-BLE");
  BLE.setDeviceName("NANO-BLE");

  // add the temperature characteristic
  sensorService.addCharacteristic(temperatureCharacteristic);

  // add the service
  BLE.addService(sensorService);

  // set initial value for this characteristic
  temperatureCharacteristic.writeValue("0.0");
  

  // start advertising
  BLE.advertise();

  Serial.println("Bluetooth® device active, waiting for connections...");
}

void loop() {
  // wait for a Bluetooth® Low Energy central
  BLEDevice central = BLE.central();

  // if a central is connected to the peripheral:
  if (central) {
    Serial.print("Connected to central: ");
    // print the central's BT address:
    Serial.println(central.address());
    // turn on the LED to indicate the connection
    digitalWrite(LED_BUILTIN, HIGH);


    // while the central is connected
    // update temperature every 200ms
    while (central.connected()) {
      long currentMillis = millis();
      if (currentMillis - previousMillis >= 200) {
        previousMillis = currentMillis;
        updateTemperature();
        
        
      }//endif 
      readGesture();

    }//end while

    // turn off the LED after disconnect
    digitalWrite(LED_BUILTIN, LOW);
    Serial.print("Disconnected from central: ");
    Serial.println(central.address());
  }
}//end loop

void updateTemperature() {
  float temperature = HS300x.readTemperature();

  if (temperature != oldTemperature) {
    char buffer[VALUE_SIZE];
    int ret = snprintf(buffer, sizeof buffer, "%f", temperature);
    if (ret >= 0) {
      temperatureCharacteristic.writeValue(buffer);
      oldTemperature = temperature;
    }
  }
}//end updateTemp

int colors[6][3] = {
    {LOW, HIGH, HIGH},  // Red (Red on)
    {LOW, LOW, HIGH},   // Yellow (Red and Green on)
    {HIGH, LOW, HIGH},  // Green (Green on)
    {HIGH, LOW, LOW},   // Cyan (Green and Blue on)
    {HIGH, HIGH, LOW},  // Blue (Blue on)
    {LOW, HIGH, LOW}    // Magenta (Red and Blue on)
};

// Define an array of color names for easier display
String colorNames[6] = { "Red", "Yellow", "Green", "Cyan", "Blue", "Magenta" };


void readGesture() {
    if (APDS.gestureAvailable()) {
        int gesture = APDS.readGesture();
        // A gesture was detected, read and process it
        currColor = -1;
        switch (gesture) {
        case GESTURE_RIGHT:
            // Cycle through colors with each RIGHT gesture
            Serial.println("Detected Right gesture");
            displayMode(gesture); // cycles forward through the color list
            break;

        case GESTURE_LEFT:
            // Cycle through colors with each RIGHT gesture
            Serial.println("Detected left gesture");
            displayMode(gesture); // cycles backward through the color list
            break;

        case GESTURE_UP:
            // Cycle through colors with each RIGHT gesture
            Serial.println("Detected Upward gesture");
            Serial.println("Entering Temperature Mode...");
            temp_Mode();
            break;

        case GESTURE_DOWN:
            // Cycle through colors with each RIGHT gesture
            Serial.println("Detected Downward gesture");
            Serial.println("Entering Display Mode...");
            displayMode(currColor);
            break;

        default:
            Serial.println("Unknown gesture detected");
            break;
        }//end switch 
    }// end if
}//end readGesture

void tempColors(float temperature) {
  if (temperature < -40.0 || temperature > 125.0) {
    Serial.println("Error: Temperature out of range!");
    return; // Exit the function if the temperature is invalid
  }

  if (temperature < 0) { // Very Cold, BLUE
    digitalWrite(LEDR, HIGH);
    digitalWrite(LEDG, HIGH);
    digitalWrite(LEDB, LOW);
    colorOut = 4;
  }
  else if (temperature < 10) { // Cold, CYAN
    digitalWrite(LEDR, HIGH);
    digitalWrite(LEDG, LOW);
    digitalWrite(LEDB, LOW);
    colorOut = 3;
  }
  else if (temperature < 25) { // Moderate, GREEN
    digitalWrite(LEDR, HIGH);
    digitalWrite(LEDG, LOW);
    digitalWrite(LEDB, HIGH);
    colorOut = 2;
  }
  else if (temperature < 35) { // Hot, YELLOW
    digitalWrite(LEDR, LOW);
    digitalWrite(LEDG, LOW);
    digitalWrite(LEDB, HIGH);
    colorOut = 1;
  }
  else { // Very Hot, RED
    digitalWrite(LEDR, LOW);
    digitalWrite(LEDG, HIGH);
    digitalWrite(LEDB, HIGH);
    colorOut = 0;
  }

  Serial.println(colorOut);
} // end tempColors

void temp_Mode() {
  float currTemp = HS300x.readTemperature();
  tempColors(currTemp);

  if (APDS.readGesture() == GESTURE_DOWN) { // enters or renters display mode if a downward function is detected
      Serial.println("Detected Downward gesture");
      Serial.println("Entering Display Mode...");
      displayMode(currColor);
  }
  delay(1000);
}//end temp_mode

void displayMode(int direction) {
    static int currentColor = 0;  // Tracks the current color index for cycling

    if (direction == GESTURE_RIGHT) {
        // Cycle through colors in forward direction
        digitalWrite(LEDR, colors[currentColor][0]);  // Set Red pin (colors[i][0])
        digitalWrite(LEDG, colors[currentColor][1]);  // Set Green pin (colors[i][1])
        digitalWrite(LEDB, colors[currentColor][2]);  // Set Blue pin (colors[i][2])
        Serial.println(currentColor);
        delay(1000); // Wait for 1000ms before changing to the next color

        // Print the current color name to the Serial Monitor
        Serial.print("Current Color: ");
        Serial.println(colorNames[currentColor]);

        // Move to the next color
        currentColor++;

        // Reset to the first color if we reach the last color
        if (currentColor >= 6) {
            currentColor = 0;  // Reset to the first color
        }
    }
    else if (direction == GESTURE_LEFT) {
        // Cycle through colors in reverse direction
        digitalWrite(LEDR, colors[currentColor][0]);  // Set Red pin (colors[i][0])
        digitalWrite(LEDG, colors[currentColor][1]);  // Set Green pin (colors[i][1])
        digitalWrite(LEDB, colors[currentColor][2]);  // Set Blue pin (colors[i][2])
        Serial.println(currentColor);
        delay(1000); // Wait for 1000ms before changing to the next color

        // Print the current color name to the Serial Monitor
        Serial.print("Current Color: ");
        Serial.println(colorNames[currentColor]);

        // Move to the previous color
        currentColor--;

        // Reset to the last color if we reach the first color
        if (currentColor < 0) {
            currentColor = 5;  // Reset to the last color (Magenta)
        }
    }
}//end displayMode