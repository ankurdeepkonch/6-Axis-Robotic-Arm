#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

Adafruit_PWMServoDriver pca(0x40);

// Calibrated pulse limits. 
// REVERSED ORDER: Motor 1 (Index 0) is now the Gripper.
struct ServoMotor {
  int channel;
  int minUs;
  int centerUs;
  int maxUs;
};

ServoMotor servos[6] = {
  {0, 600, 1500, 2400}, // Motor 1 (Gripper - MG90S) - 2400 is the math placeholder[span_0](start_span)[span_0](end_span)
  {1, 600, 1500, 2400}, // Motor 2 (Wrist - MG90S)
  {2, 600, 1500, 2400}, // Motor 3 (Upper Elbow/Logo - MG90S)
  {3, 700, 1500, 2350}, // Motor 4 (Elbow - MG996R)
  {4, 700, 1500, 2350}, // Motor 5 (Shoulder - MG996R)
  {5, 700, 1500, 2350} // Motor 6 (Base - MG996R)
};

// --- REVERSED POSES ---
// Order: {Gripper, Wrist, UpperElbow, Elbow, Shoulder, Base}
int homePose[6] = {0, 90, 90, 90, 90, 90};
int hoverPose[6] = {50, 60, 90, 0, 35, 90};
int grabPose[6] = {20, 60, 90, 60, 75, 90}; 

int currentAngles[6] = {0, 90, 90, 90, 90, 90};

void sendAngle(int motorIndex, int angle) {
  int pulse;
  if (angle <= 90) {
    pulse = map(angle, 0, 90, servos[motorIndex].minUs, servos[motorIndex].centerUs);
  } else {
    pulse = map(angle, 90, 180, servos[motorIndex].centerUs, servos[motorIndex].maxUs);
  }
  pca.writeMicroseconds(servos[motorIndex].channel, pulse);
}

void stepMotorSmoothly(int motorIndex, int targetAngle, int speedDelay = 15) {
  // NEW SAFETY LOCK: Motor 1 (Index 0) is now the gripper.
  // It restricts the angle to 90 degrees while keeping 2400 for the map() math[span_1](start_span)[span_1](end_span).
  if (motorIndex == 0) {
    targetAngle = constrain(targetAngle, 0, 90); 
  } else {
    targetAngle = constrain(targetAngle, 0, 180);
  }

  int current = currentAngles[motorIndex];
  
  if (current < targetAngle) {
    for (int a = current; a <= targetAngle; a++) {
      sendAngle(motorIndex, a);
      delay(speedDelay);
    }
  } else {
    for (int a = current; a >= targetAngle; a--) {
      sendAngle(motorIndex, a);
      delay(speedDelay);
    }
  }
  currentAngles[motorIndex] = targetAngle; 
}

void moveToPose(int targetPose[6]) {
  // Movement sequence is updated to use the new reversed motor indexes
  stepMotorSmoothly(5, targetPose[5], 15); // 1. Base (Motor 6) aligns first
  stepMotorSmoothly(2, targetPose[2], 15); // 2. Upper Elbow (Motor 3)
  stepMotorSmoothly(1, targetPose[1], 15); // 3. Wrist (Motor 2)
  stepMotorSmoothly(3, targetPose[3], 20); // 4. Elbow (Motor 4)
  stepMotorSmoothly(4, targetPose[4], 20); // 5. Shoulder (Motor 5)
  stepMotorSmoothly(0, targetPose[0], 10); // 6. Gripper (Motor 1) closes last
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  pca.begin();
  pca.setPWMFreq(50);
  delay(500);

  for (int i = 0; i < 6; i++) {
    sendAngle(i, homePose[i]);
    currentAngles[i] = homePose[i];
  }

  Serial.println("Reversed 6-Motor System Ready.");
  Serial.println("1. Type 'motor_number angle' (e.g., '1 90') to test angles slowly.");
  Serial.println("2. Type 'run' to execute the full grab sequence.");
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();



   if (input.equalsIgnoreCase("run")) {
      // --- ORIGINAL LIFTING SEQUENCE ---
      Serial.println("Sequence 1: Home Pose");
      int seq1[6] = {0, 90, 90, 90, 90, 90}; 
      moveToPose(seq1);
      delay(500);

      Serial.println("Sequence 2: Opening Gripper to 50");
      int seq2[6] = {50, 90, 90, 90, 90, 90};
      moveToPose(seq2);
      delay(500);

      Serial.println("Sequence 3: Moving down to Ball");
      // Ensure these placeholders match your actual table angles
      int seq3[6] = {50, 60, 90, 0, 35, 90}; 
      moveToPose(seq3);
      delay(500);

      Serial.println("Sequence 4: Grabbing the Ball at 20");
      int seq4[6] = {18, seq3[1], seq3[2], seq3[3], seq3[4], seq3[5]};
      moveToPose(seq4);
      delay(600);

      Serial.println("Sequence 5: Lifting to Fixed Position (60 and 75)");
      int seq5[6] = {18, seq4[1], seq4[2], 60, 75, seq4[5]}; 
      moveToPose(seq5);
      delay(500);

      // --- NEW DROP-OFF SEQUENCE ---
      Serial.println("Sequence 6: Turning Base to new Drop Zone");
      // Keeps the arm lifted, but changes the Base (Index 5) from 90 to 45 degrees
      int seq6[6] = {18, seq5[1], seq5[2], seq5[3], seq5[4], 150}; 
      moveToPose(seq6);
      delay(500);

      Serial.println("Sequence 7: Lowering to the Ground");
      // Uses the same downward elbow/shoulder angles as seq3 (100 and 120) to reach the table
      int seq7[6] = {18, seq6[1], seq6[2], 0, 35, seq6[5]}; 
      moveToPose(seq7);
      delay(500);

      Serial.println("Sequence 8: Releasing the Ball");
      // Keeps the arm down, but opens the Gripper (Index 0) to 50 degrees
      int seq8[6] = {50, seq7[1], seq7[2], seq7[3], seq7[4], seq7[5]};
      moveToPose(seq8);
      delay(500);

      Serial.println("Sequence 9: Returning to Home");
      // Lifts the arm safely back into the air with an open claw
      int seq9[6] = {0, 10, 90, 5, 120, 150};
      moveToPose(seq9);

      Serial.println("Run Complete.");
    }

    else {
      int spaceIdx = input.indexOf(' ');
      if (spaceIdx > 0) {
        int motorNum = input.substring(0, spaceIdx).toInt();
        int angle = input.substring(spaceIdx + 1).toInt();

        if (motorNum >= 1 && motorNum <= 6) {
          Serial.print("Slowly moving Motor ");
          Serial.print(motorNum);
          Serial.print(" to ");
          Serial.print(angle);
          Serial.println(" degrees...");
          
          stepMotorSmoothly(motorNum - 1, angle, 15);
        } else {
          Serial.println("Invalid motor number. Use 1 through 6.");
        }
      }
    }
  }
}
