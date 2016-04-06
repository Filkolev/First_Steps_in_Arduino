/*
 * Header here
 */

int motorPin = 13;

void setup() {
  pinMode(motorPin, OUTPUT);
}

void loop() {  
  digitalWrite(motorPin, HIGH); 
}

