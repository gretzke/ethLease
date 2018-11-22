// variables to save previous state
int xOld, yOld;

void setup() {
  Serial.begin(9600);
  // start bluetooth
  Serial1.begin(38400);
}

void loop() {
  // analog input from joystick is 10 bit (values: 0-1023)
  // only 8 bit can be sent by bluetooth module
  // divide by 8 to get 7 bit numbers
  // MSB = 0 for x, MSB = 1 for y;
  int x = analogRead(0)/8;
  int y = analogRead(1)/8;
  // set most significant bit of y to 1
  bitSet(y, 7);
  // send values via bluetooth if they change
  if (xOld != x) {
    Serial1.write(x);
  }
  if (yOld != y) {
    Serial1.write(y);
  }
  delay(100);
  xOld = x;
  yOld = y;
}
