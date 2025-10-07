#include <HardwareSerial.h>
#include <TMCStepper.h>
#include <AccelStepper.h>

#define EN_PIN_2 5
#define STEP_PIN_2 14
#define PWM_PIN 25
#define SERIAL_PORT_2 Serial2
#define DRIVER_ADDRESS_2 0b00
#define R_SENSE_2 0.15f
#define STALL_VALUE_2 2

const int PWM_CHANNEL = 0;
const int PWM_FREQ = 10000;
const int PWM_RESOLUTION = 8;
const int MAX_DUTY_CYCLE = (int)(pow(2, PWM_RESOLUTION) - 1);
const int uC_PWM = PWM_PIN;
const int DELAY_MS = 4;

const double ustep = 256;
const double revolution = 200;
const double THREADED_ROD_PITCH = 1;
bool stopFlag = false;

double syring_length;
double syring_size;
double mLBolus;
double steps;
double minutes;
double seconds;
unsigned long delayPerStepInMicroseconds;

hw_timer_t* timer1 = NULL;
TMC2209Stepper driver2(&SERIAL_PORT_2, R_SENSE_2, DRIVER_ADDRESS_2);
AccelStepper stepper1(1, 14, 23);

void setup() {
  Serial.begin(115200);
  SERIAL_PORT_2.begin(115200);
  pinMode(STEP_PIN_2, OUTPUT);
  pinMode(EN_PIN_2, OUTPUT);
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(uC_PWM, PWM_CHANNEL);
  Serial.println("ESP32 Ready to receive data");
}

void loop() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "STOP") {
      stopFlag = true;
      Serial.println("Infusion Stopped!");
    } else {
      parseSerialData(command);
      stopFlag = false;
      runBolus();
    }
  }
}

void parseSerialData(String data) {
  sscanf(data.c_str(), "Length:%lf,Size:%lf,Time:%lf:%lf",
         &syring_size, &mLBolus, &minutes, &seconds);
  Serial.println("Received Data:");
  Serial.print("Syringe Length: ");
  Serial.println(syring_length);
  Serial.print("Syringe Size: ");
  Serial.println(syring_size);
  Serial.print("Bolus per mL: ");
  Serial.println(mLBolus);
  Serial.print("Time (min:sec): ");
  Serial.print(minutes);
  Serial.print(":");
  Serial.println(seconds);
  if (syring_size == 60){
    syring_length = 90;
  }else if(syring_size == 30){
    syring_length = 80;
  }else if(syring_size == 20){
    syring_length = 70;
  }else if(syring_size == 10){
    syring_length = 57;
  }else if(syring_size == 5){
    syring_length = 42;
  }else if(syring_size == 3){
    syring_length = 46;
  }else if(syring_size == 1){
    syring_length = 57;
  }

  double ustepsPerML = (ustep * revolution * syring_length) / (syring_size * THREADED_ROD_PITCH);
  steps = mLBolus * ustepsPerML * 5;
  unsigned long totalTimeInMicroseconds = (minutes * 60 + seconds) * 1000000;
  delayPerStepInMicroseconds = totalTimeInMicroseconds / steps;
}

void runBolus() {
  if (stopFlag) return digitalWrite(EN_PIN_2, HIGH);

  digitalWrite(EN_PIN_2, LOW);
  driver2.begin();
  driver2.toff(4);
  driver2.blank_time(24);
  driver2.rms_current(500);
  driver2.mstep_reg_select(true);
  driver2.microsteps(256);
  driver2.TCOOLTHRS(0xFFFFF);
  driver2.semin(0);
  driver2.semax(2);
  driver2.shaft(true);
  driver2.sedn(0b01);
  driver2.SGTHRS(STALL_VALUE_2);
  ledcWrite(PWM_CHANNEL, 128);


  Serial.println("Bolus Started!");

  for (long i = 0; i < steps; i++) {
    if (stopFlag) {
      Serial.println("Bolus Stopped!");
      break;
    }

    digitalWrite(STEP_PIN_2, HIGH);
    delayMicroseconds(delayPerStepInMicroseconds / 2);
    digitalWrite(STEP_PIN_2, LOW);
    delayMicroseconds(delayPerStepInMicroseconds / 2);
  }

  Serial.println("Bolus is ended");
  digitalWrite(EN_PIN_2, HIGH);
}
