
#include <Wire.h>
#include <AccelStepper.h>
// Define the AccelStepper interface type; 4 wire motor in half step mode:
#define MotorInterfaceType 8

// Define number of steps per rotation:
const int stepsPerRevolution = 4096; 

// Wiring:
// Pin 8 to IN1 on the ULN2003 driver
// Pin 9 to IN2 on the ULN2003 driver
// Pin 10 to IN3 on the ULN2003 driver
// Pin 11 to IN4 on the ULN2003 driver

// Pin 4 to IN1 on the ULN2003 driver
// Pin 5 to IN2 on the ULN2003 driver
// Pin 6 to IN3 on the ULN2003 driver
// Pin 7 to IN4 on the ULN2003 driver

// Pin A4/A5 Wire 

// Pin 2 Test Button



// Create stepper object called 'myStepper1', note the pin order:
//Stepper myStepper1 = Stepper(stepsPerRevolution, 8, 10, 9, 11);

AccelStepper myStepper1 = AccelStepper(MotorInterfaceType, 8, 10, 9, 11);
AccelStepper myStepper2 = AccelStepper(MotorInterfaceType, 4, 6, 5, 7);

const int buttonTest = 2;

int currentPos1 = 0;
int currentPos2 = 0;

int motor;
char motorMode;
int steps=0;
bool gotOne=false;

const int whichModule = 1; //1 or 2
//TODO: Have an Input pin for this so same code.
//      Have PRoper calibration.
void setup()
{
  Serial.begin(9600);           // start serial for output
  Serial.print("Startup ");
  Serial.println(whichModule);

  Wire.begin(whichModule);                // join i2c bus with address #1 or#2 depeending on which module
  Wire.onReceive(receiveEvent); // register event

  myStepper1.setMaxSpeed(1000);
  myStepper1.setCurrentPosition(currentPos1);
  myStepper1.setAcceleration(200);

  myStepper2.setMaxSpeed(1000);
  myStepper2.setCurrentPosition(currentPos2);
  myStepper2.setAcceleration(200);

  pinMode(buttonTest,INPUT_PULLUP);

}

void loop()
{
  delay(100);
  int buttonStateTest = digitalRead(buttonTest);

  if (buttonStateTest == LOW) {
    Serial.println("Test ButtonPressed");
    // turn LED on:
      //myStepper1.step(stepsPerRevolution);
     while (myStepper2.currentPosition() != stepsPerRevolution) {
        Serial.print("Test Pos:");
        myStepper2.setSpeed(1000);

        Serial.println(myStepper2.currentPosition());
        myStepper2.runSpeed();
      }
      delay(500);
  } 

  //This stepper library can actually move things simultaneosly but for now
  //lets concentrate on 1 at a time, this wont push the power supply either.
  

  if (gotOne){
    gotOne=false; 
      Serial.print("received:");
      Serial.print(motor);
      Serial.print(":");
      Serial.print(motorMode);
      Serial.print(":");
      Serial.println(steps);

    
    if(motorMode == 'R') {
      Serial.println("Reset Motor Pos");
      if (motor==1) {
        currentPos1 = 0;
        myStepper1.setMaxSpeed(1000);
        myStepper1.setCurrentPosition(currentPos1);
        myStepper1.setAcceleration(200);
      } else {
        currentPos2 = 0;
        myStepper2.setMaxSpeed(1000);
        myStepper2.setCurrentPosition(currentPos2);
        myStepper2.setAcceleration(200);
      }
    }

    //Turn motorMode
    if(motorMode == 'T') {
       Serial.print("Going clock- Pos=");      

      if (motor==1) {
        currentPos1=steps;
        Serial.println(currentPos1);
        myStepper1.moveTo(currentPos1);
        // Run to position with set speed and acceleration:
        myStepper1.runToPosition();
        myStepper1.stop();
      }else {
        currentPos2=steps;
        Serial.println(currentPos2);
        myStepper2.moveTo(currentPos2);
        // Run to position with set speed and acceleration:
        myStepper2.runToPosition();
        myStepper2.stop();      
      }
    }
  
  }

}

// function that executes whenever data is received from master
// this function is registered as an event, see setup()
// Dont run motors in this code it will go bang.
void receiveEvent(int howMany)
{
  Serial.println("receivedEvent IC2");
  union Buffer
  {
     int intNumber;
     byte intBytes[2];
  };
  Buffer recBuffer;
  
  motor = Wire.read(); 
  motorMode = Wire.read(); // receive byte as a character
  if (Wire.available() > 0)   // slave may send less than requested
   {
      recBuffer.intBytes[0] = Wire.read();
      recBuffer.intBytes[1] = Wire.read();
   }
  steps = recBuffer.intNumber;    // receive byte as an integer

  gotOne=true;



}
