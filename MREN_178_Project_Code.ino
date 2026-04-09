#include <LiquidCrystal.h>
// LCD Pin Definitions
#define LCD_WIDTH   16
#define LCD_HEIGHT  2
#define PINRS       8
#define PINEN       9
#define PIND4       4 
#define PIND5       5 
#define PIND6       6 
#define PIND7       7  
//Hardware And System Definitions
#define BUTTONPIN   A0
#define BUZZERPIN   13
#define ELEVATORSIZE 16

LiquidCrystal lcd(PINRS, PINEN, PIND4, PIND5, PIND6, PIND7);
//FSM and direction states
enum FSMState {IDLE, MOVING_UP, MOVING_DOWN, DOOR_OPEN, DOOR_CLOSE, EMERGENCY_STOP};
enum Direction {DOWN = 0, UP = 1};
//Elevator/system State Tracking
struct Elevator {
  FSMState currentState;
  Direction currentDir;
  int currentFloor;
} elevator;

// Arrays and Queue capacity tracking
int upQueue[ELEVATORSIZE];
int upCount = 0;

int downQueue[ELEVATORSIZE];
int downCount = 0; 

// Timer variable to track 20 second idle time
unsigned long lastActivityTime = 0;

/*
  Queue insertions management and sorting
  Up queue gets sorted in ascending order
  Down queue gets sorted in decending order 
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
/*
  Insertion for up and down queue requests
  Protects the program against array overflow and duplicate enties before sorting
*/
void insertUpRequest(int floorNum){
  if (upCount >= ELEVATORSIZE) return; // prevents memory overflow

  for (int i = 0; i < upCount; i++){
    if (upQueue[i] == floorNum) return; // Reject fuplicate floor requests
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
  Popping requests by extracting a specific index from the queue and shifting all remaining elements to close the gap
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
  Wired to the select button on the LCD screen, if triggered lock the program in an infinite while loop untill reset
*/
void checkEmergencyStop() {
  int btnValue = analogRead(BUTTONPIN);
  // Analog voltage for select button press on LCD screen
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
      delay(100); // Traps in infinite loop until hardware resart
    }
  }
}

/*
  DOOR ANIMATION HELPER
  Handles the buzzer sound and timing, LCD updating, and idle state logic
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
  delay(3000); // Ooens doors for passengers

  // Edge Case: keeps the doors open and idle if the elevator is at the lobby floor 1
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
  
  lastActivityTime = millis(); // reset idle timer
}

/*
  NON-BLOCKING MOVEMENT
  Moves the elevator 1 step and redraws the LCD screen
*/
void moveOneFloor(int step) {
  // Check if doors were left open at Floor 1, and if open close before moving
  if (elevator.currentState == DOOR_OPEN){
    elevator.currentState = DOOR_CLOSE;
    lcd.setCursor(0, 1);
    lcd.print("Doors Closing  ");
    tone(BUZZERPIN, 800, 400);
    delay(1500);
  }

  checkEmergencyStop(); // emergency stop check before completing a movement

  lcd.clear();
  elevator.currentFloor += step; // math for moving up or down movmeent
  
  // Draw shaft for the elevator
  for (int i = elevator.currentFloor; i < 16; i++){
    lcd.setCursor(i, 0);
    lcd.print("-");
  }

  // Draw elevator as a solid block
  lcd.setCursor(elevator.currentFloor - 1, 0);
  lcd.write(255);

  // Draw text for direction and current floor number
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
  // initialize starting state
  elevator.currentState = DOOR_OPEN;
  elevator.currentDir = UP;
  elevator.currentFloor = 1;
  // Draw the initial UI
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
  // check emergency button press first
  checkEmergencyStop();

  // return to the lobby first floor if completely idle for 20 seconds
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
    lastActivityTime = millis(); // reset time while requests are still active
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

        // routing logic
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
          // Inside the elevator: route by math based on current elevator location
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
      // 1. Scan for the closest floor ahead of the elevator current floor
      int targetIndex = -1;
      for (int i = 0; i < upCount; i++) {
        if (upQueue[i] >= elevator.currentFloor) {
          targetIndex = i;
          break; // closest valid target index is found break
        }
      }

      // 2. move to the target and service it
      if (targetIndex != -1) {
        elevator.currentState = MOVING_UP;
        int targetFloor = upQueue[targetIndex];
        
        if (elevator.currentFloor == targetFloor) {
          popSpecificUpRequest(targetIndex); 
          cycleDoors(); // arrived at desired floor
        } else {
          moveOneFloor(1); // Take one step up
        }
      } else {
        // 3. change queue logic: when no targets ahead of current floor, sweep is finished
        if (downCount > 0) {
          elevator.currentDir = DOWN; // Switch to Down Queue
        } else {
          // Edge Case: if up requests exist but they are below the elevators current floor ignore them and pick them up on the next sweep
          // Force the downward movement to pick up the passengers wanting to go down
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
  // downward sweeping logic
  else if (elevator.currentDir == DOWN){
    if (downCount > 0){
      // 1. Scan for the closest floor the elevator
      int targetIndex = -1;
      // downQueue is sorted descending 
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
          moveOneFloor(-1); // Take 1 step down
        }
      } else {
        // Switch directions
        if (upCount > 0) {
          elevator.currentDir = UP; 
        } else {
          // Edge Case: down requests exist but they are above the elevators current floor
          // Ignore them for the next sweep and start serving up requests
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