/*
 * Description:
 * 
 * The setup simulates a relatively simple system in which an energy source 
 * emits a variable amount of energy which is stored in a pool with limited 
 * capacity. The collected energy is used to power a motor which performs a 
 * useful task. If the energy in the pool is too low or too high the motor 
 * will shut down to prevent damage. The user can exert some control over 
 * the system by regulating the motor’s consumption, turning it ON or OFF 
 * and releasing energy from the pool. In order to decide on the best course 
 * of action to keep the system running, various sensors provide useful 
 * information about the system’s state. 
 * 
 * For more information see: http://tiny.cc/yfglay
 * 
 * Author: Filip Kolev
 * Date: April 2016
 * 
 * Changelog
 * ==================================
 * 2016-04-06 - Filip Kolev
 *    Created project
 *    Added project description document (in progress)
 *    Added env_limits.h header containing environment constants
 *    Added pins.h header containing pin definitions  
 */

#include <stdio.h>

#include "env_limits.h"
#include "pins.h"

const String infoMessageTemplate = 
          "I: Motor [%s, %d%% speed, %s], Pool [%s (%d / %d)], " 
          "Net Inflow [%d (%d inflow, %d consumed, %d released)], " 
          "Valve [%d%% open]";

int poolEnergy = 0;
int prevPoolEnergy = 0;
int netGain;
int releaseValveReading;

int onButtonState = LOW;
int onButtonReading;
int onButtonPrevReading = LOW;

int offButtonState = LOW;
int offButtonReading;
int offButtonPrevReading = LOW;

int toggleButtonState = LOW;
int toggleButtonReading;
int toggleButtonPrevReading = LOW;

int motorRegulatorPotReading;
int motorSpeed = 0;
bool isMotorRotatingClockwise = true;

int energySourceReading;

void setup() {
  Serial.begin(BAUD_RATE);

  pinMode(LOW_ENERGY_LED, OUTPUT);
  pinMode(OK_ENERGY_LED, OUTPUT);
  pinMode(HIGH_ENERGY_LED, OUTPUT);
  pinMode(CRITICAL_ENERGY_LED, OUTPUT);

  pinMode(NET_GAIN_LED, OUTPUT);
  pinMode(NET_LOSS_LED, OUTPUT);

  pinMode(ON_BUTTON, INPUT);
  pinMode(OFF_BUTTON, INPUT);

  pinMode(MOTOR_SPEED_PIN, OUTPUT);
  pinMode(MOTOR_1A_PIN, OUTPUT);
  pinMode(MOTOR_2A_PIN, OUTPUT); 

  pinMode(MOTOR_OFF_SIGNAL, OUTPUT);
}

void loop() {
  collectEnergyFromEnvironment();
  checkUserIntervention();
  expendEnergy();  
  signalEnergyLevelAndNetGain();

  if (motorSpeed == ZERO_SPEED) {
    signalMotorOff();
  }

  if (millis() % LOG_INFO_INTERVAL ==0) {
    logSystemState();
  }
}

void collectEnergyFromEnvironment(void) {
  energySourceReading = analogRead(ENERGY_SOURCE);
  poolEnergy += energySourceReading;
}

void checkUserIntervention(void) {
  checkOnButtonPressed();
  checkOffButtonPressed();
  checkToggleButtonPressed();
}

void expendEnergy(void) {
  releaseValveReading = analogRead(RELEASE_VALVE);
  poolEnergy -= motorSpeed + releaseValveReading;
}

void signalEnergyLevelAndNetGain(void) {
  if (poolEnergy == POOL_EMPTY) {
    signalLowEnergy(true);    
  } if (poolEnergy <= POOL_LOW_HIGHER) {
    signalLowEnergy(false);
    enterMotorLowPowerMode();
  } else if (poolEnergy < POOL_HIGH_LOWER) {
    signalOptimalEnergy();
  } else if (poolEnergy < POOL_CRITICAL_LOWER) {
    signalHighEnergy();
  } else if (poolEnergy < POOL_FULL) {
    signalCriticalEnergy(false);
  } else {
    signalCriticalEnergy(true);    
  }
}

void signalMotorOff(void) {
  // TODO
}

void logSystemState(void) {
  // TODO
}

void checkOnButtonPressed(void) {
  // TODO
}

void checkOffButtonPressed(void) {
  // TODO
}

void checkToggleButtonPressed(void) {
  // TODO
}

void signalLowEnergy(bool powerDownMotor) {
  digitalWrite(OK_ENERGY_LED, LOW);
  digitalWrite(HIGH_ENERGY_LED, LOW);
  digitalWrite(CRITICAL_ENERGY_LED, LOW);

  blinkLed(LOW_ENERGY_LED, LOW_ENERGY_TIMEOUT);

  if (powerDownMotor) {
    turnOffMotor();
    Serial.println("<E> Motor shut down. Reason: low energy.");
  }
}

void enterMotorLowPowerMode(void) {
  motorSpeed = QUARTER_SPEED;
  Serial.println("<W> Motor entered low power mode. Speed is now at 25% of max.");
}

void signalOptimalEnergy(void) {
  digitalWrite(LOW_ENERGY_LED, LOW);
  digitalWrite(HIGH_ENERGY_LED, LOW);
  digitalWrite(CRITICAL_ENERGY_LED, LOW);

  if (motorSpeed == ZERO_SPEED) { // Motor off
    blinkLed(OK_ENERGY_LED, OK_ENERGY_TIMEOUT);
  } else {
    digitalWrite(OK_ENERGY_LED, HIGH);
  }
}

void signalHighEnergy(void) {
  digitalWrite(LOW_ENERGY_LED, LOW);
  digitalWrite(OK_ENERGY_LED, LOW);
  digitalWrite(CRITICAL_ENERGY_LED, LOW);

  blinkLed(HIGH_ENERGY_LED, HIGH_ENERGY_TIMEOUT);
}

void signalCriticalEnergy(bool powerDownMotor) {
  digitalWrite(LOW_ENERGY_LED, LOW);
  digitalWrite(OK_ENERGY_LED, LOW);
  digitalWrite(HIGH_ENERGY_LED, LOW);

  blinkLed(CRITICAL_ENERGY_LED, CRITICAL_ENERGY_TIMEOUT);

  if (powerDownMotor) {
    turnOffMotor();
    Serial.println("<E> Motor shut down. Reason: high energy.");
  }
}

void turnOffMotor(void) {
  motorSpeed = ZERO_SPEED;  
}

void blinkLed(int led, int timeout) {
  // TODO
  // lastLedChange to coordinate blink?
}

