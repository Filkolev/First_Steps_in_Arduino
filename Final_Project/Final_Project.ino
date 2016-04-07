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
 *    Added random number generation to produce spurious energy inflows
 * 2016-04-07    
 *    Implemented signalMotorOff()
 *    Implemented button checks and debounce
 *    Work in progress: implemented motor control
 *    Implemented pool status LED blinking
 *    Implemented logging
 */

#include <stdio.h>

#include "env_limits.h"
#include "pins.h"

const char* infoMessageTemplate = 
          "I: Motor [%s, %d%% speed, %s], Pool [%s (%d / %d)], " 
          "Net Inflow [%d (%d inflow, %d consumed, %d released)], " 
          "Valve [%d%% open]";
          
char *logMessage;

long poolEnergy = 0;
char poolState[9] = {0};
int prevPoolEnergy = 0;

long totalInflow = 0;
long prevInflow = 0;
long totalConsumed = 0;
long prevConsumed = 0;
long totalReleased = 0;
long prevReleased = 0;

int releaseValveReading;

int onButtonState = LOW;
int onButtonReading;
int onButtonPrevState = LOW;
long onButtonLastDebounce = 0;

int offButtonState = LOW;
int offButtonReading;
int offButtonPrevState = LOW;
long offButtonLastDebounce = 0;

int toggleButtonState = LOW;
int toggleButtonReading;
int toggleButtonPrevState = LOW;
long toggleButtonLastDebounce = 0;

int motorRegulatorPotReading;
int motorSpeed = 0;
bool isMotorRotatingClockwise = true;
bool isMotorInLowPowerMode = false;

int energySourceReading;

long lastLedChange = 0;
int blinkLedState = LOW;

void setup() {
  Serial.begin(BAUD_RATE);
  randomSeed(analogRead(A5));

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

  if (millis() % LOG_INFO_INTERVAL == 0) {
    logSystemState();
  }
}

void collectEnergyFromEnvironment(void) {
  energySourceReading = analogRead(ENERGY_SOURCE);
  poolEnergy += energySourceReading;

  if (millis() % RANDOM_INFLOW_INTERVAL == 0) {
    poolEnergy += random(RANDOM_INFLOW_MAX + 1);
  }

  if (poolEnergy > POOL_FULL) {
    poolEnergy = POOL_FULL;
  }
}

void checkUserIntervention(void) {
  checkOnButtonPressed();
  checkOffButtonPressed();
  checkToggleButtonPressed();
}

void expendEnergy(void) {
  releaseValveReading = analogRead(RELEASE_VALVE);
  runMotor(false);
  poolEnergy -= motorSpeed + releaseValveReading;

  if (poolEnergy < POOL_EMPTY) {
    poolEnergy = POOL_EMPTY;
  }
}

void signalEnergyLevelAndNetGain(void) {
  isMotorInLowPowerMode = false;
  
  if (poolEnergy == POOL_EMPTY) {
    signalLowEnergy(true);   
    strcpy(poolState, "Empty");
  } else if (poolEnergy < POOL_LOW_HIGHER) {
    signalLowEnergy(false);
    strcpy(poolState, "Low"); 
    enterMotorLowPowerMode();
  } else if (poolEnergy < POOL_HIGH_LOWER) {
    signalOptimalEnergy();
    strcpy(poolState, "OK");         
  } else if (poolEnergy < POOL_CRITICAL_LOWER) {
    signalHighEnergy();
    strcpy(poolState, "High"); 
  } else if (poolEnergy < POOL_FULL) {
    signalCriticalEnergy(false);
    strcpy(poolState, "Critical"); 
  } else {
    signalCriticalEnergy(true); 
    strcpy(poolState, "Full");    
  }

  if (poolEnergy > prevPoolEnergy) {
    analogWrite(
      NET_GAIN_LED, 
      (poolEnergy - prevPoolEnergy) / ANALOG_READ_WRITE_COEFFICIENT);
  } else {
    analogWrite(
      NET_LOSS_LED, 
      (prevPoolEnergy - poolEnergy) / ANALOG_READ_WRITE_COEFFICIENT);
  }
}

void signalMotorOff(void) {
  digitalWrite(MOTOR_OFF_SIGNAL, HIGH);
}

void logSystemState(void) { 
  int motorSpeedPercent = 0;

  switch(motorSpeed) {
    case ZERO_SPEED:
      motorSpeedPercent = 0;
      break;
    case QUARTER_SPEED:
      motorSpeedPercent = 25;
      break;
    case HALF_SPEED:
      motorSpeedPercent = 50;
      break;
    case THREE_QUARTERS_SPEED:
      motorSpeedPercent = 75;
      break;
    case FULL_SPEED:
      motorSpeedPercent = 100;
      break;    
  }
  
  int valveOpenPercentage = 100 * releaseValveReading / MAX_ANALOG_READ;

  sprintf(
    logMessage, 
    infoMessageTemplate, 
    motorSpeed == ZERO_SPEED ? "OFF" : "ON",
    motorSpeedPercent,
    isMotorRotatingClockwise ? "Clockwise" : "Counter-clockwise",
    poolState,
    poolEnergy,
    POOL_FULL,
    poolEnergy - prevPoolEnergy,
    totalInflow - prevInflow,
    totalConsumed - prevConsumed,
    totalReleased - prevReleased,
    valveOpenPercentage);

  prevPoolEnergy = poolEnergy;  
  prevInflow = totalInflow;
  prevConsumed = totalConsumed;
  prevReleased = totalReleased;
}

void checkButtonPressed(
  int button, 
  int *state, 
  int *prevState, 
  int *reading, 
  long *lastDebounce, 
  void (*func)(void)){
    *reading = digitalRead(button);

    if (*reading != *prevState) {
      *lastDebounce = millis();
    }

    if (millis() - *lastDebounce >= DEBOUNCE_INTERVAL) {
      if (*reading != *state) {
        *state = *reading;
      }

      if (*state == HIGH) {
        func();
      }
    }

    *prevState = *reading;
  }

void checkOnButtonPressed(void) {
  checkButtonPressed(
    ON_BUTTON, 
    &onButtonState, 
    &onButtonPrevState,
    &onButtonReading, 
    &onButtonLastDebounce,
    turnOnMotor); 
}

void checkOffButtonPressed(void) {
  checkButtonPressed(
    OFF_BUTTON, 
    &offButtonState, 
    &offButtonPrevState,
    &offButtonReading, 
    &offButtonLastDebounce,
    turnOffMotor);
}

void checkToggleButtonPressed(void) {
  checkButtonPressed(
    TOGGLE_BUTTON, 
    &toggleButtonState, 
    &toggleButtonPrevState,
    &toggleButtonReading, 
    &toggleButtonLastDebounce,
    toggleMotorDirection);
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
  isMotorInLowPowerMode = true;
  Serial.println("<W> Motor entered low power mode. Speed is now at 25% of max.");
}

void signalOptimalEnergy(void) {
  digitalWrite(LOW_ENERGY_LED, LOW);
  digitalWrite(HIGH_ENERGY_LED, LOW);
  digitalWrite(CRITICAL_ENERGY_LED, LOW);

  if (motorSpeed == ZERO_SPEED) { 
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
  if (motorSpeed == ZERO_SPEED) {
    Serial.println("<W> Received OFF signal. Motor is already OFF. Nothing to do.");
    return;
  }
  
  motorSpeed = ZERO_SPEED;  
}

void turnOnMotor(void) {
  if (motorSpeed != ZERO_SPEED) {
    Serial.println("<W> Receved ON signal. Motor is already ON. Nothing to do.");
    return;
  }
  
  runMotor(true);
}

void toggleMotorDirection(void) {
  if (motorSpeed == ZERO_SPEED) {
    Serial.println("<W> Received TOGGLE signal. Motor is OFF. Nothing to do.");
    return;
  }
  
  isMotorRotatingClockwise = !isMotorRotatingClockwise;
}

void runMotor(bool turnMotorOn) {
  if (motorSpeed != ZERO_SPEED || turnMotorOn) {
    adjustMotorSpeed();
  }  

  // TODO: Verify direction conforms to documentation
  digitalWrite(MOTOR_1A_PIN, isMotorRotatingClockwise ? LOW : HIGH);
  digitalWrite(MOTOR_2A_PIN, isMotorRotatingClockwise ? HIGH : LOW);
  analogWrite(MOTOR_SPEED_PIN, motorSpeed);
}

void adjustMotorSpeed(void) {
  if (isMotorInLowPowerMode) {
    motorSpeed = QUARTER_SPEED;
    return;
  }
  
  motorRegulatorPotReading = analogRead(MOTOR_REGULATOR);

  if (motorRegulatorPotReading <= QUARTER_SPEED_CTRL_MAX) {
    motorSpeed = QUARTER_SPEED;
  } else if (motorRegulatorPotReading <= HALF_SPEED_CTRL_MAX) {
    motorSpeed = HALF_SPEED;
  } else if (motorRegulatorPotReading <= THREE_QUARTERS_SPEED_CTRL_MAX) {
    motorSpeed = THREE_QUARTERS_SPEED;
  } else {
    motorSpeed = FULL_SPEED;
  }
}

void blinkLed(int led, unsigned int timeout) {
  if(millis() - lastLedChange >= timeout) {
    blinkLedState = !blinkLedState;
    digitalWrite(led, blinkLedState);
    lastLedChange = millis();
  }
}

