#include <string.h>
#include <Wire.h>
#include <math.h>

#define NANO_ID 0

#define LENGTH_HEX_ANGLE 2
#define LENGTH_PWM_COMMAND 4
#define DIGITS_PWM_FEEDBACK 3
#define Kp 1.0 // Gain for proportional controller
#define Ki 0.5 // Gain for integrator
#define DELTA 7 // freezing regions at crossing area

#define RECEIVE_PWM_CMD 'p'
#define RECEIVE_FEEDBACK_REQUEST 'f'
#define RECEIVE_TEST_REQUEST 't'
#define RECEIVE_TESTDRIVE_REQUEST 'z'

#define BAUD_RATE 74880

#define MOTOR_PIN 2
/////////////////////////// DEBUGGING AND TIMING VARIABLES //////////////

unsigned long int t_ref;

/////////////////////////// MOTORS DATA BANK //////////////
int maximumPWMFeedback[8] = {1501, 1494, 1501, 1493, 1501, 1499, 1501, 1520};
int minimumPWMFeedback[8] = {484, 483, 484, 482, 484, 484, 484, 491};
int middlePWMFeedback[8] = {992, 988, 992, 987, 992, 991, 992, 1005}; // all numbers rounded down
int maximumPWMOutput[8] = {1488, 1485, 1489, 1481, 1488, 1490, 1490, 1509};
int minimumPWMOutput[8] = {469, 469, 471, 473, 469, 471, 474, 481}; //3, 6, 7 increased by 5
int clockwise_max[8] = {2194, 2175, 2185, 2175, 2189, 2188, 2188, 2215};
int clockwise_min[8] = {2094, 2082, 2090, 2079, 2089, 2088, 2088, 2117};
int clockwise_max_speed[8] = {791, 777, 764, 756, 752, 780, 770, 796};
int clockwise_min_speed[8] = {374, 364, 370, 363, 345, 360, 370, 373};
int anticlockwise_max[8] = {1791, 1780, 1785, 1780, 1785, 1786, 1788, 1811};
int anticlockwise_min[8] = {1891, 1880, 1887, 1876, 1885, 1886, 1888, 1910};
int anticlockwise_max_speed[8] = { 281, 278, 273, 269, 270, 279, 273, 279}; //All are negative values
int anticlockwise_min_speed[8] = { 365, 366, 356, 335, 352, 358, 365, 372}; //All are negative values

/////////////////////////// SERVO PARAMETERS ///////////////////////// //

/// PWM scale for position feedback from servo ///
double stepPWMFeedback = (double)(maximumPWMFeedback[NANO_ID] - minimumPWMFeedback[NANO_ID]) / 1440.0; //360 degree in quarter degree precision -> 1440 steps

/// PWM scale for position output to servo ///
double stepPWMOutput = (double)(maximumPWMOutput[NANO_ID] - minimumPWMOutput[NANO_ID]) / 1440.0; //360 degree in quarter degree precision -> 1440 steps

double initialDeg = -1;

int angularChangeReceived;
int angularChangeFeedback;

int lastDeg;
int currentDeg = minimumPWMFeedback[NANO_ID];
double delayTime;

char pwmFeedback[DIGITS_PWM_FEEDBACK];

/////////////////////////// FEEDBACK VARIABLES ///////////////////////////

/// servo position as 'pwm' value (see _feedback for scale above) ///
int servoPWM;

/////////////////////////// OUTPUT VARIABLES ///////////////////////////

/// derived aim of angle ///
int destinationDeg = 0;
int destinationPWM;

/// control communication with serial and servo ///
boolean firstTimeRead = 1;
boolean enableServo = 0;
boolean stillmode = 0;
boolean positiveCommand = 0;
boolean positiveFeedback = 0;

boolean cw = 1;
int pwmTestrun = 700;

int lastPWM = 0;
int pwmCommand = 0;


/////////////////////////// FUNCTION PRECALLING ///////////////////////////

//void setup_timer1();
void sendFeedback();
void readSerial();
int readPositionFeedback();
void quitCrossing();
void ctrl_motor(int pwmMotor);
void servoPulse(int pulseWidth);

/////////////////////////// CONTROL VARIABLES ///////////////////////////

/// controlling of the method crossing() ///
int cross = 0;
int crossPulse = 0;
boolean crossCW;

/// control for errors ///
double ref, ITerm, lastErr, dInput;
unsigned long lastTime;
String strReceived;

void setup() {
  Serial.begin(BAUD_RATE);
  readPositionFeedback();
  lastPWM = servoPWM - 13;
  Serial.println(lastPWM);
}

void loop() {
  readSerial();
}

void readSerial() //receive characterizing prefix (+ length in 2 digit Hex, with manipulation of first bit for sign)
{
  if (Serial.available() > 0) {
    strReceived = Serial.readStringUntil('\n');
    char command = strReceived[0];
    if (command == RECEIVE_PWM_CMD) { //p
      readPositionFeedback();
      if ((cross > 0) && (cross < 3)) {
        quitCrossing();
      }
      readPWMCommand();
      ctrl_motor(pwmCommand);
    }
    else if (command == RECEIVE_FEEDBACK_REQUEST) { //f
      sendFeedback();
    }
    else if (command == RECEIVE_TEST_REQUEST) { //t
      Serial.print('c');
      Serial.println('c');
      Serial.flush();
      delayMicroseconds(10);
    }
    else if (command == RECEIVE_TESTDRIVE_REQUEST) { //z
      if (cw) {
        if (pwmTestrun < (maximumPWMOutput[NANO_ID] + 20)) {
          pwmTestrun += 20;
        }
        else {
          pwmTestrun -= 20;
          cw = 0;
        }
      } else {
        if (pwmTestrun > (minimumPWMOutput[NANO_ID] - 20)) {
          pwmTestrun -= 20;
        }
        else {
          pwmTestrun += 20;
          cw = 1;
        }
      }
      Serial.print('c');
      Serial.println('c');
      Serial.flush();
      delayMicroseconds(10);

      digitalWrite(MOTOR_PIN, HIGH);
      delayMicroseconds(pwmTestrun);
      digitalWrite(MOTOR_PIN, LOW);
      delayMicroseconds(3000 - pwmTestrun);
    }
  }
  // ADD CALIBRATION LATER
}

void readPWMCommand() {
  char pwmReceived[LENGTH_PWM_COMMAND];
  cross = strReceived[1 + (LENGTH_PWM_COMMAND * NANO_ID)] - '0';
  //Serial.print("cross ");
  //Serial.println(cross);
  for (int i = 0; i < LENGTH_PWM_COMMAND - 1; i++) {
    pwmReceived[i] = strReceived[1 + i + 1 + (LENGTH_PWM_COMMAND * NANO_ID)]; //+1 for prefix, another +1 to omit the crossing boolean
  }
  pwmCommand = strtol(pwmReceived, 0, 16);
  if ((cross > 0) && (cross < 3)) {
    if (cross == 1) { //->CW crossing
      crossCW = true;
    } else if (cross == 2) {
      crossCW = false;
    }
  }
}

int readPositionFeedback()
{ //reads position feedback and stores it in servoPWM
  int lastPWM = servoPWM; //temporarily stores last value as backup, if new measurement fails - not used anywhere else

  digitalWrite(MOTOR_PIN, HIGH);
  delayMicroseconds(50);
  digitalWrite(MOTOR_PIN, LOW);
  servoPWM = pulseIn(MOTOR_PIN, HIGH, 2000); //triggers servo, then measures time until next HIGH signal, cuts off after 3000us or 3ms

  if ((servoPWM < 300) || (servoPWM > 2000)) { //results outside these boundaries are faulty
    servoPWM = lastPWM;
  }
  else if (servoPWM < minimumPWMFeedback[NANO_ID]) {
    servoPWM = minimumPWMFeedback[NANO_ID];
  }
  else if (servoPWM > maximumPWMFeedback[NANO_ID]) {
    servoPWM = maximumPWMFeedback[NANO_ID];
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void crossing() {
  if (crossCW) { //->CW crossing
    int PWMdifference = pwmCommand - lastPWM;
    //delayTime = (double)(PWMdifference * 1000 / anticlockwise_max_speed[NANO_ID]);
    delayTime = 150;
    // Serial.print(" CW ");
    //Serial.println(PWMdifference);
  } else if (!crossCW)
  { //->CCW crossing
    int PWMdifference = lastPWM - pwmCommand;
    //delayTime = (double)(PWMdifference * 1000 / clockwise_max_speed[NANO_ID]);
    delayTime = 150;
    //Serial.print(" CCW ");
    //Serial.println(PWMdifference);
  } else
  {
    cross = false;
  }
  lastPWM = pwmCommand;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void quitCrossing() {
  //Serial.print("servoPWM");
    //Serial.println(servoPWM);
  if (!crossCW) // From right
  {     
    if ((servoPWM < (maximumPWMFeedback[NANO_ID] - DELTA)) && (servoPWM > middlePWMFeedback[NANO_ID] )) {
      cross = 0;
    }
  }
  else if (crossCW) // From left
  {
    if ((servoPWM > DELTA) && (servoPWM <= 720 )) { //smaller/equal because middlePWMFEedback is rounded down
      cross = 0;
    }
  }
}

void ctrl_motor(int pwmMotor) { //transmits the output signal towards the motor
  if ((cross > 0) && (cross < 3))
  {
    if (crossCW) {
      crossPulse = clockwise_max[NANO_ID];
    } else if (!crossCW) {
      crossPulse = anticlockwise_max[NANO_ID];
    }
    //Serial.print("crossPulse");
    //Serial.println(crossPulse);
    servoPulse(crossPulse);
    servoPulse(crossPulse);
    //delay(500);
    //cross = 0;
    //servoPulse(pwmMotor);
    //servoPulse(pwmMotor);
    //Serial.print("pwmMotor");
    //Serial.println(pwmMotor);
  }
  else {
    servoPulse(pwmMotor);
    servoPulse(pwmMotor);
    //Serial.println(pwmMotor);
  }
}

void servoPulse(int pulseWidth) {
  digitalWrite(MOTOR_PIN, HIGH);
  delayMicroseconds(pulseWidth);
  digitalWrite(MOTOR_PIN, LOW);
  delayMicroseconds(3000 - pulseWidth);
}


void sendFeedback() {
  itoa(servoPWM, pwmFeedback, 16);
  for (int i = 0; i < DIGITS_PWM_FEEDBACK; i++) {
    Serial.print(pwmFeedback[i]);
    Serial.flush();
  }
}
