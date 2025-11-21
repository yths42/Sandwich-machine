#include <ESP32Servo.h>
#include <Stepper.h>

const int stepsPerRevolution = 2048;  // 28BYJ-48
const int stepsMotorSkalGa = 2992;    // 34*88

// --- Servoer ---
Servo myServo1;
Servo myServo2;
const int servoPin1 = 18;
const int servoPin2 = 19;

// --- Knapp (til GND, intern pull-up) ---
const int buttonPin = 4;

// --- Stepper pinner (ULN2003) ---
// pins brød dytter
#define IN1 21
#define IN2 22
#define IN3 23
#define IN4 25

// pins syltetøy
#define IN5 26
#define IN6 27
#define IN7 32
#define IN8 33

Stepper brodDytter(stepsPerRevolution, IN1, IN3, IN2, IN4);
Stepper syltetoyKlemmer(stepsPerRevolution, IN5, IN7, IN6, IN8);

int buttonState;
int lastButtonState;  // forrige knapp-status

// --- Hjelpefunksjoner ---

// Jiggle når BARE brød dytter skal gå fremover.
// Nettoposisjon etterpå = totalSteps (f.eks. 2640).
void brodDyttJiggleAlene(int totalSteps) {
  long twoThirds = (totalSteps * 2) / 3;     // ~2/3 frem
  long oneQuarter = totalSteps / 4;          // 1/4 tilbake
  long finalFwd = totalSteps - (twoThirds - oneQuarter); // resten til full frem

  // frem ~2/3
  brodDytter.step((int)twoThirds);
  // tilbake 1/4
  brodDytter.step(-(int)oneQuarter);
  // frem til full
  brodDytter.step((int)finalFwd);
}

// Jiggle når brød dytter går fremover SAMTIDIG som syltetøy går tilbake.
// Holder jevn bevegelse ved å kjøre i små "chunks".
void brodDyttJiggleMedSyltetoy(int totalSteps, int chunkBread = 10, int chunkJam = 5, int delayMs = 15) {
  // chunkJam/chunkBread = 0.5, matcher ca. den tidligere 15 pr 30
  long twoThirds = (totalSteps * 2) / 3;     
  long oneQuarter = totalSteps / 4;          
  long finalFwd = totalSteps - (twoThirds - oneQuarter);

  auto stepSegment = [&](long segSteps, int breadDir) {
    long remaining = segSteps;
    int jamDir = +1; // syltetøy går tilbake (samme retning som før: positivt talte steg)
    // kjør i "chunks" for jevn bevegelse
    while (remaining > 0) {
      int stepNow = (remaining >= chunkBread) ? chunkBread : (int)remaining;
      // brød
      brodDytter.step(breadDir * stepNow);
      // syltetøy (holder 1/2 forhold)
      int jamStepsNow = (stepNow * chunkJam) / chunkBread; // skal bli 5 når stepNow=10
      if (jamStepsNow == 0 && stepNow > 0) jamStepsNow = 1; // sikrer litt bevegelse
      syltetoyKlemmer.step(jamDir * jamStepsNow);
      delay(delayMs);
      remaining -= stepNow;
    }
  };

  // frem ~2/3
  stepSegment(twoThirds, +1);
  // tilbake 1/4 (brød litt tilbake, syltetøy fortsetter tilbake)
  stepSegment(oneQuarter, -1);
  // frem til full
  stepSegment(finalFwd, +1);
}

void setup() {
  Serial.begin(115200);

  // Servoer til luken
  myServo1.attach(servoPin1);
  myServo2.attach(servoPin2);

  // Knapp (intern pullup = HIGH når ikke trykket)
  pinMode(buttonPin, INPUT_PULLUP);

  buttonState = digitalRead(buttonPin);
  lastButtonState = buttonState;
  Serial.print("Start knapp-state: ");
  Serial.println(buttonState);

  // Stepper-hastighet (RPM)
  brodDytter.setSpeed(10);
  syltetoyKlemmer.setSpeed(7);

  Serial.println("Init OK");
}

void loop() {
  // Les knapp
  lastButtonState = buttonState;
  buttonState = digitalRead(buttonPin);

  // Sjekker om knappen nettopp BLE trykket (HIGH -> LOW)
  if (lastButtonState == HIGH && buttonState == LOW) {
    Serial.println("Knapp trykket, starter sekvens");

    // --- 1) Brød dyttes frem med jiggle (alene) til nett +2640 ---
    brodDyttJiggleAlene(stepsMotorSkalGa);

    // --- 2) Syltetøy klemmes, samtidig som brød dytter går tilbake (vanlig tilbake) ---
    for (int angle = 88; angle > 0; angle--) {
      syltetoyKlemmer.step(-15);
      brodDytter.step(-34);
      delay(15);
    }

    // --- 3) Luke åpnes ---
    for (int angle = 0; angle < 88; angle++) {
      myServo1.write(angle);
      myServo2.write(angle);
      delay(10);
    }

    delay(500);

    // --- 4) Luke lukkes ---
    for (int angle = 88; angle > 0; angle--) {
      myServo1.write(angle);
      myServo2.write(angle);
      delay(10);
    }

    // --- 5) Nytt brød dyttes frem med jiggle, mens syltetøy går tilbake jevnt ---
    syltetoyKlemmer.setSpeed(10);
    brodDyttJiggleMedSyltetoy(stepsMotorSkalGa, /*chunkBread=*/10, /*chunkJam=*/10, /*delayMs=*/20);

    delay(500);

    // --- 6) Luke åpnes igjen ---
    for (int angle = 0; angle < 88; angle++) {
      myServo1.write(angle);
      myServo2.write(angle);
      delay(15);
    }

    // --- 7) Luke lukkes, brød dytter går vanlig tilbake ---
    for (int angle = 88; angle > 0; angle--) {
      myServo1.write(angle);
      myServo2.write(angle);
      brodDytter.step(-34);
      delay(15);
    }

    // Vent til knappen SLIPPES før vi godtar nytt trykk
    while (digitalRead(buttonPin) == LOW) {
      delay(10);
    }
    delay(50);
  }
}
