/*
   Description:

   The setup simulates a relatively simple system in which an energy source
   emits a variable amount of energy which is stored in a pool with limited
   capacity. The collected energy is used to power a motor which performs a
   useful task. If the energy in the pool is too low or too high the motor
   will shut down to prevent damage. The user can exert some control over
   the system by regulating the motor’s consumption, turning it ON or OFF
   and releasing energy from the pool. In order to decide on the best course
   of action to keep the system running, various sensors provide useful
   information about the system’s state.

   For more information see: http://tiny.cc/yfglay

   Author: Filip Kolev
   Date: April 2016

   Changelog
   ==================================
   2016-04-06 - Filip Kolev
      Created project
      Added project description document (in progress)
      Added env_limits.h header containing environment constants
      Added pins.h header containing pin definitions
      Added random number generation to produce spurious energy inflows
   2016-04-07 - Filip Kolev
      Implemented signalMotorOff()
      Implemented button checks and debounce
      Work in progress: implemented motor control
      Implemented pool status LED blinking
      Implemented logging
      TODO: Fix environment limits; shut down motor
   2016-04-08 - Filip Kolev
      Added stability timeouts to make system changes smoother
      Modified environment variables
      Fixed entering low power mode logging
      Added function to exit low power mode
      Testing and buf fixes
      Added more log messages
      Fixed net inflow LEDs' behaviour
*/

#include "env_limits.h"
#include "pins.h"

long poolEnergy = 0;
char poolState[9] = {0};
long prevPoolEnergy = 0;

long totalInflow = 0;
long prevInflow = 0;
long totalConsumed = 0;
long prevConsumed = 0;
long totalReleased = 0;
long prevReleased = 0;
int rateOfNetChange;

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
int motorSpeed = ZERO_SPEED;
bool isMotorRotatingClockwise = true;
bool isMotorInLowPowerMode = false;

int energySourceReading;

long lastLedChange = 0;
int blinkLedState = LOW;

// Log stability
long prevInfoLog = 0;
long prevRandomEnergyGain = 0;
long prevStabilityCheck = 0;
long prevLowPowerModeLog = 0;
long currentMillis;

void setup() {
  Serial.begin(BAUD_RATE);
  
  // Seed the random generator with readings from an unused analog pin
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
  currentMillis = millis();
  if (currentMillis - prevStabilityCheck < STABILITY_TIMEOUT) {
    return;
  }

  prevStabilityCheck = currentMillis;

  collectEnergyFromEnvironment();
  checkUserIntervention();
  expendEnergy();  
  signalEnergyLevelAndNetGain();

  if (motorSpeed == ZERO_SPEED) {
    signalMotorOff();
  }

  currentMillis = millis();
  if (currentMillis - prevInfoLog >= LOG_INFO_INTERVAL) {
    logSystemState();
    prevInfoLog = currentMillis;
  }
}

void collectEnergyFromEnvironment(void) {
  energySourceReading = MAX_ANALOG_READ - analogRead(ENERGY_SOURCE);
  poolEnergy += energySourceReading;
  totalInflow += energySourceReading;

  if (millis() - prevRandomEnergyGain >= RANDOM_INFLOW_INTERVAL) {
    poolEnergy += random(RANDOM_INFLOW_MAX + 1);
    prevRandomEnergyGain = millis();
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

  totalReleased += min(poolEnergy, releaseValveReading);
  totalConsumed += min(poolEnergy, motorSpeed);
  poolEnergy -= motorSpeed + releaseValveReading;

  if (poolEnergy < POOL_EMPTY) {
    poolEnergy = POOL_EMPTY;
  }
}

void signalEnergyLevelAndNetGain(void) {
  if (poolEnergy < POOL_LOW_LOWER) {
    signalLowEnergy(true);
    strcpy(poolState, "Empty");
  } else if (poolEnergy < POOL_LOW_HIGHER) {
    signalLowEnergy(false);
    strcpy(poolState, "Low");
    if (motorSpeed != ZERO_SPEED
        && !isMotorInLowPowerMode
        && currentMillis - prevLowPowerModeLog >= LOW_POWER_MODE_LOG_TIMEOUT) {
      enterLowPowerMode();
      prevLowPowerModeLog = currentMillis;
    }
  } else if (poolEnergy < POOL_HIGH_LOWER) {
    signalOptimalEnergy();
    strcpy(poolState, "OK");
    exitLowPowerMode();
  } else if (poolEnergy < POOL_CRITICAL_LOWER) {
    signalHighEnergy();
    strcpy(poolState, "High");
    exitLowPowerMode();
  } else if (poolEnergy < POOL_FULL_LOWER) {
    signalCriticalEnergy(false);
    strcpy(poolState, "Critical");
    exitLowPowerMode();
  } else {
    signalCriticalEnergy(true);
    strcpy(poolState, "Full");
    exitLowPowerMode();
  }

  if (poolEnergy > prevPoolEnergy) {
    signalRateChange(NET_GAIN_LED, NET_LOSS_LED, poolEnergy - prevPoolEnergy);
  } else {
    signalRateChange(NET_LOSS_LED, NET_GAIN_LED, prevPoolEnergy - poolEnergy);
  }
}

void signalRateChange(int ledOn, int ledOff, long diff) {  
  currentMillis = millis();

  // Stabilize signals to correspond to info logs
  if(currentMillis - prevInfoLog < LOG_INFO_INTERVAL) {
    return;
  }

  // Map the power of illumination based on empirical tests
  rateOfNetChange = map(
                      diff,
                      0,
                      INFLOW_ABS_MAX,
                      MIN_ANALOG_WRITE,
                      MAX_ANALOG_WRITE);

  analogWrite(ledOn, rateOfNetChange);
  analogWrite(ledOff, MIN_ANALOG_READ);
}

void signalMotorOff(void) {
  digitalWrite(MOTOR_OFF_SIGNAL, HIGH);
}

void logSystemState(void) {
  int motorSpeedPercent = getMotorSpeedPercent();
  long valveOpenPercentage = 100L * releaseValveReading / MAX_ANALOG_READ;

  Serial.print("<I> Motor [");
  Serial.print(motorSpeed == ZERO_SPEED ? "OFF" : "ON");
  Serial.print(", ");
  Serial.print(motorSpeedPercent);
  Serial.print("% speed, ");
  Serial.print(isMotorRotatingClockwise ? "Clockwise" : "Counter-clockwise");
  Serial.print("], Pool [");
  Serial.print(poolState);
  Serial.print(" (");
  Serial.print(poolEnergy);
  Serial.print(" / ");
  Serial.print(POOL_FULL);
  Serial.print(")], Net Inflow [");
  Serial.print(poolEnergy - prevPoolEnergy);
  Serial.print(" (");
  Serial.print(totalInflow - prevInflow);
  Serial.print(" inflow, ");
  Serial.print(totalConsumed - prevConsumed);
  Serial.print(" consumed, ");
  Serial.print(totalReleased - prevReleased);
  Serial.print(" released)], Valve [");
  Serial.print(valveOpenPercentage);
  Serial.println("% open]");

  prevPoolEnergy = poolEnergy;
  prevInflow = totalInflow;
  prevConsumed = totalConsumed;
  prevReleased = totalReleased;
}

int getMotorSpeedPercent(void) {
  switch (motorSpeed) {
    case ZERO_SPEED:
      return 0;
    case QUARTER_SPEED:
      return 25;
    case HALF_SPEED:
      return 50;
    case THREE_QUARTERS_SPEED:
      return 75;
    case FULL_SPEED:
      return 100;
    default:
      return 0; // Shouldn't ever reach this, but just to be safe
  }
}

void checkButtonPressed(
  int button,
  int *state,
  int *prevState,
  int *reading,
  long *lastDebounce,
  void (*func)(void)) {
  currentMillis = millis();
  *reading = digitalRead(button);

  if (*reading != *prevState) {
    *lastDebounce = currentMillis;
  }

  if (currentMillis - *lastDebounce >= DEBOUNCE_INTERVAL) {
    if (*reading != *state) {
      *state = *reading;

      if (*state == HIGH) {
        func();
      }
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

  currentMillis = millis();
  if (powerDownMotor && motorSpeed != ZERO_SPEED) {
    prevLowPowerModeLog = currentMillis;
    turnOffMotor();
    Serial.println("<E> Motor shut down. Reason: low energy.");
  }
}

void enterLowPowerMode(void) {
  isMotorInLowPowerMode = true;
  Serial.println("<W> Motor entered low power mode. Speed is now at 25% of max.");
}

void exitLowPowerMode(void) {
  if (motorSpeed != ZERO_SPEED && isMotorInLowPowerMode) {
    isMotorInLowPowerMode = false;
  }
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

  if (powerDownMotor && motorSpeed != ZERO_SPEED) {
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
  Serial.println("<I> Received OFF signal. Motor turned OFF.");
}

void turnOnMotor(void) {
  if (motorSpeed != ZERO_SPEED) {
    Serial.println("<W> Receved ON signal. Motor is already ON. Nothing to do.");
    return;
  }

  if (poolEnergy <= POOL_LOW_HIGHER || POOL_HIGH_LOWER < poolEnergy) {
    Serial.println("<E> Receved ON signal. Energy in pool is not within safe limits. Signal ignored.");
    return;
  }

  Serial.println("<I> Receved ON signal. Motor tuned ON.");
  runMotor(true);
}

void toggleMotorDirection(void) {
  if (motorSpeed == ZERO_SPEED) {
    Serial.println("<W> Received TOGGLE signal. Motor is OFF. Nothing to do.");
    return;
  }

  isMotorRotatingClockwise = !isMotorRotatingClockwise;
  Serial.print("<I> Received TOGGLE signal. Motor now rotates ");
  Serial.println(isMotorRotatingClockwise ? "clockwise." : "counter-clockwise.");
}

void runMotor(bool turnMotorOn) {
  if (motorSpeed != ZERO_SPEED || turnMotorOn) {    
    adjustMotorSpeed();
  }

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
  currentMillis = millis();

  if (currentMillis - lastLedChange >= timeout) {
    blinkLedState = !blinkLedState;
    digitalWrite(led, blinkLedState);
    lastLedChange = currentMillis;
  }
}

