#include <LiquidCrystal.h>

#define LCD_WIDTH   16
#define LCD_HEIGHT  2

#define PINRS       8
#define PINEN       9
#define PIND4       4 
#define PIND5       5 
#define PIND6       6 
#define PIND7       7  
#define BUTTONPIN   A0
#define BUZZERPIN   13
#define ELEVATORSIZE 10

LiquidCrystal lcd(PINRS, PINEN, PIND4, PIND5, PIND6, PIND7);

enum FSMState {IDLE, MOVING_UP, MOVING_DOWN, DOOR_OPEN, DOOR_CLOSE, EMERGENCY_STOP};
enum Direction {DOWN = 0, UP = 1};

struct Elevator {
  FSMState currentState;
  Direction currentDir;
  int currentFloor;
} elevator;

// Arrays and Queue tracking
int upQueue[ELEVATORSIZE];
int upCount = 0;

int downQueue[ELEVATORSIZE];
int downCount = 0; 

// Timer variable to track idle time
unsigned long lastActivityTime = 0;

/*
  Queue insertions management and sorting
*/
void insertionSort(int arr[], size_t n, Direction d) {
  for (int i = 1; i < n; i++) {
    int key = arr[i];
    int j = i - 1;

    if (d == UP) {
      while (j >= 0 && arr[j] > key) {
        arr[j + 1] = arr[j];
        j--;
      }
    } 
    else {
      while (j >= 0 && arr[j] < key) {
        arr[j + 1] = arr[j];
        j--;
      }
    }
    arr[j + 1] = key;
  }
}

void insertUpRequest(int floorNum){
  if (upCount >= ELEVATORSIZE) return; 

  for (int i = 0; i < upCount; i++){
    if (upQueue[i] == floorNum) return;
  }

  upQueue[upCount] = floorNum;
  upCount++;
  insertionSort(upQueue, upCount, UP);
}

void insertDownRequest(int floorNum){
  if (downCount >= ELEVATORSIZE) return; 

  for (int i = 0; i < downCount; i++){
    if (downQueue[i] == floorNum) return;
  }

  downQueue[downCount] = floorNum;
  downCount++;
  insertionSort(downQueue, downCount, DOWN);
}

/* 
  Popping requests
*/
int popSpecificUpRequest(int index) {
  if (upCount == 0 || index < 0 || index >= upCount) return -1;
  int nextFloor = upQueue[index];
  
  // Shift everything down to close the gap
  for (int i = index; i < upCount - 1; i++){
    upQueue[i] = upQueue[i + 1];
  }
  upCount--;
  return nextFloor;
}

int popSpecificDownRequest(int index) {
  if (downCount == 0 || index < 0 || index >= downCount) return -1;
  int nextFloor = downQueue[index];
  
  for (int i = index; i < downCount - 1; i++){
    downQueue[i] = downQueue[i + 1];
  }
  downCount--;
  return nextFloor;
}

/*
  EMERGENCY STOP HELPER
*/
void checkEmergencyStop() {
  int btnValue = analogRead(BUTTONPIN);
  
  if (btnValue > 600 && btnValue < 800) {
    elevator.currentState = EMERGENCY_STOP;
  }

  if (elevator.currentState == EMERGENCY_STOP) {
    Serial.println("EMERGENCY STOP ACTIVATED");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("EMERGENCY!");
    lcd.setCursor(0, 1);
    lcd.print("PRESS RESET BTN");
    while (true) {
      delay(100); 
    }
  }
}

/*
  DOOR ANIMATION HELPER
*/
void cycleDoors() {
  elevator.currentState = DOOR_OPEN;
  lcd.setCursor(0, 1);
  lcd.print("Doors Opening  ");
  tone(BUZZERPIN, 1000, 200);
  delay(300);
  tone(BUZZERPIN, 1000, 200);
  delay(1000);

  lcd.setCursor(0, 1);
  lcd.print("Doors Open     ");
  delay(3000);

  // Check if we stay open at Floor 1
  if (upCount == 0 && downCount == 0 && elevator.currentFloor == 1){
    lcd.setCursor(0, 1);
    lcd.print("Idle: Doors Open");
  }
  else {
    elevator.currentState = DOOR_CLOSE;
    lcd.setCursor(0, 1);
    lcd.print("Doors Closing  ");
    tone(BUZZERPIN, 800, 400);
    delay(1500);

    elevator.currentState = IDLE;
    lcd.setCursor(0, 1);
    lcd.print("Elevator Ready ");
  }
  
  lastActivityTime = millis();
}

/*
  NON-BLOCKING MOVEMENT: Take exactly 1 step and return
*/
void moveOneFloor(int step) {
  // Check if doors were left open at Floor 1
  if (elevator.currentState == DOOR_OPEN){
    elevator.currentState = DOOR_CLOSE;
    lcd.setCursor(0, 1);
    lcd.print("Doors Closing  ");
    tone(BUZZERPIN, 800, 400);
    delay(1500);
  }

  checkEmergencyStop();

  lcd.clear();
  elevator.currentFloor += step;
  
  // Draw shaft
  for (int i = elevator.currentFloor; i < 16; i++){
    lcd.setCursor(i, 0);
    lcd.print("-");
  }

  // Draw elevator
  lcd.setCursor(elevator.currentFloor - 1, 0);
  lcd.write(255);

  // Draw text
  lcd.setCursor(0, 1);
  lcd.print("Floor: ");
  lcd.print(elevator.currentFloor);
  if (step == 1) lcd.print(" ->");
  else lcd.print(" <-");

  delay(1000); // Wait 1 second for travel time
}


void setup() {
  pinMode(BUTTONPIN, INPUT);
  pinMode(BUZZERPIN, OUTPUT); 

  Serial.begin(9600);
  lcd.begin(LCD_WIDTH, LCD_HEIGHT);

  elevator.currentState = DOOR_OPEN;
  elevator.currentDir = UP;
  elevator.currentFloor = 1;

  lcd.setCursor(0, 0);
  for (int i = 0; i < 16; i++){
    lcd.setCursor(i, 0);
    lcd.print("-");
  }
  lcd.setCursor(elevator.currentFloor - 1, 0);
  lcd.write(255);

  Serial.println("Elevator FSM initialized");
  lcd.setCursor(0,1);
  lcd.print("Idle: Doors Open");

  lastActivityTime = millis();
}

void loop() {
  checkEmergencyStop();

  // IDLE TIMEOUT LOGIC
  if (upCount == 0 && downCount == 0){
    if (elevator.currentFloor != 1){
      elevator.currentState = IDLE;
    }

    if ((millis() - lastActivityTime >=  20000) && elevator.currentFloor != 1){
      Serial.println("Idle: returning to Floor 1");
      insertDownRequest(1);
    } 
  }
  else {
    lastActivityTime = millis();
  }

  // SERIAL PARSING
  while (Serial.available() > 0){
    String inputCMD = Serial.readStringUntil('\n'); 
    inputCMD.trim(); 

    int spaceIndex = inputCMD.indexOf(' ');

    if (spaceIndex != -1){ 
      int floorNum = inputCMD.substring(spaceIndex + 1).toInt(); 
      String command = inputCMD.substring(0, spaceIndex); 

      bool validCommand = command.equalsIgnoreCase("Up") || command.equalsIgnoreCase("Down") || command.equalsIgnoreCase("Floor");

      if (floorNum < 1 || floorNum > 16 || !validCommand) {
        Serial.println("Invalid Request");
      }
      else {
        Serial.print("Word: "); Serial.println(command);
        Serial.print("Number: "); Serial.println(floorNum); 

        
        if (command.equalsIgnoreCase("Up")) {
          Serial.println("Hall Call: Place in UP queue");
          insertUpRequest(floorNum);
          if (elevator.currentState == IDLE) elevator.currentDir = UP;
        } 
        else if (command.equalsIgnoreCase("Down")) {
          Serial.println("Hall Call: Place in DOWN queue");
          insertDownRequest(floorNum);
          if (elevator.currentState == IDLE) elevator.currentDir = DOWN;
        } 
        else {
          // Inside the elevator: route by math
          if (floorNum > elevator.currentFloor){
            Serial.println("Car Call: Place in UP queue");
            insertUpRequest(floorNum);
            if (elevator.currentState == IDLE) elevator.currentDir = UP;
          }
          else if (floorNum < elevator.currentFloor){
            Serial.println("Car Call: Place in DOWN queue");
            insertDownRequest(floorNum);
            if (elevator.currentState == IDLE) elevator.currentDir = DOWN;
          }
        }
      }
    }
    else {
      Serial.println("Invalid request");
    }
  }

  // NON-BLOCKING MOVEMENT LOGIC
  if (elevator.currentDir == UP){
    if (upCount > 0){
      // 1. Scan for the closest floor AHEAD of us
      int targetIndex = -1;
      for (int i = 0; i < upCount; i++) {
        if (upQueue[i] >= elevator.currentFloor) {
          targetIndex = i;
          break; // Found the closest valid target in the upward sweep
        }
      }

      // 2. Execute movement based on scan
      if (targetIndex != -1) {
        elevator.currentState = MOVING_UP;
        int targetFloor = upQueue[targetIndex];
        
        if (elevator.currentFloor == targetFloor) {
          popSpecificUpRequest(targetIndex); 
          cycleDoors();
        } else {
          moveOneFloor(1); // Force upward step
        }
      } else {
        // No UP requests ahead of us! The sweep is done.
        if (downCount > 0) {
          elevator.currentDir = DOWN; // Switch to Down Queue
        } else {
          // We have UP requests, but they are BELOW us (deferred to next cycle).
          // We must travel down to reset the sweep.
          elevator.currentState = MOVING_DOWN;
          moveOneFloor(-1); 
        }
      }
    }
    else if (downCount > 0){
      elevator.currentDir = DOWN;
      Serial.println("Switching to down queue");
    }
  }

  else if (elevator.currentDir == DOWN){
    if (downCount > 0){
      // 1. Scan for the closest floor BELOW us
      int targetIndex = -1;
      // downQueue is sorted descending (e.g., 10, 5, 2)
      for (int i = 0; i < downCount; i++) {
        if (downQueue[i] <= elevator.currentFloor) {
          targetIndex = i;
          break; 
        }
      }

      // 2. Execute movement based on scan
      if (targetIndex != -1) {
        elevator.currentState = MOVING_DOWN;
        int targetFloor = downQueue[targetIndex];
        
        if (elevator.currentFloor == targetFloor) {
          popSpecificDownRequest(targetIndex); 
          cycleDoors();
        } else {
          moveOneFloor(-1); // Force downward step
        }
      } else {
        // No DOWN requests below us! The sweep is done.
        if (upCount > 0) {
          elevator.currentDir = UP; 
        } else {
          // We have DOWN requests, but they are ABOVE us.
          // Travel up to reset the sweep.
          elevator.currentState = MOVING_UP;
          moveOneFloor(1); 
        }
      }
    }
    else if (upCount > 0){
      elevator.currentDir = UP;
      Serial.println("Switching to up queue");
    }
  }
}