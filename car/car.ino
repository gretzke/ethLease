// util libraries
#include "LedControl.h"
#include <Servo.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#define BUFFER_SIZE 512
// wifi libraries
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

HTTPClient http;

// define GPIO pins
#define RX D7

#define LED_CS D3
#define LED_CLK D4
#define LED_DIN D5

#define SERVO_PIN D6
#define enA D2
#define in1 D0
#define in2 D1

#define SMARTCONTRACT_SWITCH D8

// define software serial ports for bluetooth module (HC-05)
SoftwareSerial BTSerial(RX, SW_SERIAL_UNUSED_PIN); // RX | TX (no pin is used for transmit)

// LED 8x8
LedControl lc = LedControl(LED_DIN, LED_CLK, LED_CS, 1); // Pins: DIN,CLK,CS, # of Display connected

// Servo
Servo servo;

// x - angle of servo
// y - speed of dc motor
int x, y;

/*
 * WIFI
 */

// wifi credentials
char ssid[] = ""; // SSID of your Wifi Router
char pass[] = "";              // Password of your Wifi Router

// node and contract address, calldata and fingerprint for https requests
char nodeAddr[] = "https://rinkeby.infura.io/";
char contractAddr[] = "";
char callData[] = "0x2bb33104";
// fingerprint in case "https://rinkeby.infura.io/" is used as a host
char fingerprint[] = "0B:FE:65:E8:C1:7A:9E:C2:8D:98:5B:BA:1A:EC:7A:4B:83:68:DE:11";

// char array to store body for https call
char ethCallBuffer[BUFFER_SIZE];
// speed multiplier based on state of smart contract
float speedMultiplier;

/*
 * LED 8x8
 */

byte leaseActive[] = {
  B00000000,
  B00000001,
  B00000010,
  B00000100,
  B10001000,
  B01010000,
  B00100000,
  B00000000
};

byte leaseExpiring[] = {
  B00111100,
  B01000010,
  B10010001,
  B10010001,
  B10011101,
  B10000001,
  B01000010,
  B00111100
};

byte leaseAwaitingPayment[] = {
  B10000001,
  B01000010,
  B00100100,
  B00011000,
  B00011000,
  B00100100,
  B01000010,
  B10000001
};

byte error[] = {
  B00011000,
  B00011000,
  B00011000,
  B00011000,
  B00011000,
  B00000000,
  B00011000,
  B00011000
};

/*
 * functions to set LED
 * also sets speed multiplier
 */

void setActive() {
  for (int i = 0; i < 8; i++) {
    lc.setRow(0, i, leaseActive[i]);
  }
  speedMultiplier = 1;
}

void setExpiring() {
  for (int i = 0; i < 8; i++) {
    lc.setRow(0, i, leaseExpiring[i]);
  }
  speedMultiplier = 0.5;
}

void awaitingPayment() {
  for (int i = 0; i < 8; i++) {
    lc.setRow(0, i, leaseAwaitingPayment[i]);
  }
  speedMultiplier = 0;
}

void setError() {
  for (int i = 0; i < 8; i++) {
    lc.setRow(0, i, error[i]);
  }
  speedMultiplier = 0;
}

/*
 * Servo
 */

// function to set the servo motor that controls the steering
void setServo(int deg) {
  // make sure that the degree is between 90 and 180 degrees, otherwise the servo might brake
  if (deg < 90) {
    deg = 90;
  } else if (deg > 180) {
    deg = 180;
  }
  servo.write(deg);
  delay(10);
}

/*
 * HTTPS call functions
 */

// function to generate JSON objects for web3 https calls
// inputs char array of method name, json array of parameters and pointer to variable to store result in
void generateWeb3Call(char methodName[], JsonArray &params, char (*result)[BUFFER_SIZE]) {
  StaticJsonBuffer<BUFFER_SIZE> JSONbuffer;
  // generate JSON object for RPC call
  // ('{"jsonrpc":"2.0","method":"...",call","params":[{...}],"id":1}')
  JsonObject &JSONencoder = JSONbuffer.createObject();
  JSONencoder["jsonrpc"] = "2.0";
  JSONencoder["method"] = methodName;
  JSONencoder["params"] = params;
  JSONencoder["id"] = 1;
  JSONencoder.printTo(*result, sizeof(*result));
}

// Function to execute web3 rpc calls
// returns string of the result
String web3Call(char body[]) {
  // http POST
  http.begin(nodeAddr, fingerprint);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(body);
  String payload = http.getString();
  http.end();

  if (httpCode > 0) {
    // parse JSON and return result
    StaticJsonBuffer<BUFFER_SIZE> JSONbuffer;
    JsonObject &result = JSONbuffer.parseObject(payload);
    
    if (result.containsKey("result")) {
      return result["result"];
    } else if (result.containsKey("error")) {
      Serial.println(result["error"].as<String>());
      return "error";
    }
  }
  else {
    Serial.println(http.errorToString(httpCode).c_str());
    return "error";
  }
}

/*
 * SETUP
 */

void setup() {
  // setup LED 8x8
  lc.shutdown(0, false); // Wake up displays
  lc.shutdown(1, false);
  lc.setIntensity(0, 5); // Set intensity levels
  lc.setIntensity(1, 5);
  lc.clearDisplay(0); // Clear Displays
  lc.clearDisplay(1);
  setActive();
  // start servo
  servo.attach(SERVO_PIN);
  // start bluetooth
  BTSerial.begin(38400);
  // setup wifi
  Serial.begin(115200);
  delay(10);
  // Connect to Wi-Fi network
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Wi-Fi connected successfully");

  // generate eth call params for rpc calls
  // parameters consist of a transaction call object and the block number
  // https://github.com/ethereum/wiki/wiki/JSON-RPC#parameters-24
  StaticJsonBuffer<BUFFER_SIZE> callJSONbuffer;
  JsonArray &callParams = callJSONbuffer.createArray();

  JsonObject &callValues = callJSONbuffer.createObject();
  callValues["to"] = contractAddr;
  callValues["data"] = callData;

  callParams.add(callValues);
  callParams.add("latest");

  // generate JSON string once on setup, to reuse in loop
  generateWeb3Call("eth_call", callParams, &ethCallBuffer);

  // setup dc motor
  pinMode(enA, OUTPUT);
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  // Set initial rotation direction to forward
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
}

/*
 * LOOP
 */

void loop() {
  // get smart contract state every 15 seconds
  static const unsigned long refreshIntervall = 15000; // ms
  static unsigned long lastRefreshTime = 0;
  // check if smart contract mode is activated
  if (digitalRead(SMARTCONTRACT_SWITCH)) {
    if (millis() - lastRefreshTime >= refreshIntervall) {
      lastRefreshTime += refreshIntervall;
      // call web3
      String result = web3Call(ethCallBuffer);
      /*
       * result should either be:
       * 0x0000000000000000000000000000000000000000000000000000000000000000
       * 0x0000000000000000000000000000000000000000000000000000000000000001
       * 0x0000000000000000000000000000000000000000000000000000000000000002
       */
      if (result != "error") {
        // parse int from last character
        int state = result.substring(result.length() - 1, result.length()).toInt();
        Serial.println(state);
        switch (state) {
        case 0:
          setActive();
          break;
        case 1:
          setExpiring();
          break;
        case 2:
          awaitingPayment();
          break;
        }
      }
      else {
        setError();
      }
    }
  } else {
    setActive();
    lastRefreshTime = millis() - refreshIntervall;
  }

  // if there is data incoming from the bluetooth serial port
  if (BTSerial.available()) {
    int num = BTSerial.read();
    // read most significant bit
    // if MSB = 1, we have the y value, otherwise the x value
    if (bitRead(num, 7)) {
      // replace the MSB with a 0
      bitWrite(num, 7, 0);
      // convert the y value
      y = map(num, 0, 127, -64, 63);
      // if y smaller than 0 set motor to reverse
      if (y < 0) {
        digitalWrite(in1, LOW);
        digitalWrite(in2, HIGH);
        // map y value to 0-1023 and write it to motor
        y = map(abs(y), 1, 64, 0, 1023);
        // apply speed multiplier from smart contract
        y = y * speedMultiplier;
        analogWrite(D2, y);
        // else set motor to forward
      } else {
        digitalWrite(in1, HIGH);
        digitalWrite(in2, LOW);
        // map y value to 0-1023 and write it to motor
        y = map(y, 0, 63, 0, 1023);
        // apply speed multiplier from smart contract
        y = y * speedMultiplier;
        analogWrite(D2, y);
      }
    } else {
      // convert to servo angle range
      x = map(num, 0, 127, 90, 180);
      setServo(x);
    }
    Serial.print("X: ");
    Serial.print(x);
    Serial.print(", Y: ");
    Serial.println(y);
  }
}
