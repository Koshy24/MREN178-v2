#include <LiquidCrystal.h>

#include <DS3231.h>

#define LCD_WIDTH   16
#define LCD_HEIGHT  2
#define LCD_SIZE    (LCD_WIDTH * LCD_HEIGHT)

#define PINRS       8
#define PINEN       9
#define PIND4       4 
#define PIND5       5 
#define PIND6       6 
#define PIND7       7  
#define BUTTONPIN   A0
#define ELEVATORSIZE 10

LiquidCrystal lcd(PINRS, PINEN, PIND4, PIND5, PIND6, PIND7);

enum FSMState {IDLE, MOVING_UP, MOVING_DOWN, DOOR_OPEN, DOOR_CLOSE, EMERGENCY_STOP};
enum Direction {DOWN = 0, UP = 1};

struct Elevator {
  FSMState currentState;
  Direction currentDir;
  int currentFloor;
} elevator;

// Initialize Array based queue size for requests counters keep track of number of requests in either up or down direction
int upQueue[ELEVATORSIZE];
int upCount = 0;

int downQueue[ELEVATORSIZE];
int downCount = 0; 

void insertUpRequest(int floorNum){
  if (upCount >= ELEVATORSIZE) return; // prevent array overflow

  for (int i = 0; i < upCount; i++){
    if (upQueue[i] == floorNum) return;
  }

  upQueue[upCount] = floorNum
  upCount++;
  
  insertionSort(upQueue, upCount, UP);
}

void insertDownRequest(int floorNum){
  if (downCount >= ELEVATORSIZE) return; // prevent array overflow

  for (int i = 0; i < downCount; i++){
    if (downQueue[i] == floorNum) return;
  }

  downQueue[downCount] = floorNum;
  downCount++;

  insertionSort(downQueue, downCount, DOWN);
}

/* 
  Poping requests to move to 
*/

int popUpRequest(){
  if (upCount == 0) return -1;
  
  int nextFloor = upQueue[0];

  for (int i = 0; i < upCount; i++){
    upQueue[i] = upQueue[i + 1];
  }

  upCount--;

  return nextFloor - 1;

}

int popDownRequest(){
  if (downCount == 0) return -1;

  int nextFloor = upQueue[0];

  for (int i = 0; i < downCount; i++){
    downQueue[i] = downQueue[i + 1];
  }

  downCount--;

  return nextFloor - 1;

}

/*
  insertion sort for sorting queues in sequential order
*/

void insertionSort(int arr[], size_t n, Direction d) {
  for (int i = 1; i < n; i++) {
    int key = arr[i];
    int j = i - 1;

    if (d == UP) {
      // Ascending Order (e.g., 2, 4, 5)
      while (j >= 0 && arr[j] > key) {
        arr[j + 1] = arr[j];
        j--;
      }
    } 
    
    else {
      // Descending Order 
      while (j >= 0 && arr[j] < key) {
        arr[j + 1] = arr[j];
        j--;
      }
    }
    arr[j + 1] = key;
  }
}

/*
  lcd animation for floors
*/
void moveToFloor(int target){
  if (target == -1 || target == elevator.currentFloor) return;

  int step = (target>elevator.currentFloor) ? 1: -1;

  lcd.setCursor(0, 1);
  lcd.print("Moving to Floor: " + target);

  while (target != elevator.currentFloor){
    /* 
    EMERGENCY STOP CODE - While moving
    */
    if(digitalRead(BUTTONPIN) == LOW){ //switch the LOW to HIGH based on wiring of button 
      elevator.currentState = EMERGENCY_STOP; 
    }

    if(elevator.currentState == EMERGENCY_STOP){
      Serial.println("EMERGENCY STOP ACTIVATED");

      lcd.clear();
      lcd.setcursor(0,0);
      lcd.print("!!! EMERGENCY !!!");
    
      return; 
    }

    lcd.clear();
    elevator.currentFloor += step;
    lcd.setCursor(elevator.currentFloor, 0);
    lcd.write(255);
    for (int i = elevator.currentFloor + 1; i < 16; i++){
      lcd.setCursor(i, 0);
      lcd.print("-");
    }
    _delay_ms(1000);
  }
}

void setup() {
  
  /*
    LCD startup and initial elevator setup
  */
  pinMode(BUTTONPIN, INPUT_PULLUP); 

  Serial.begin(9600);
  lcd.begin(LCD_WIDTH, LCD_HEIGHT);

  elevator.currentState = IDLE;
  elevator.currentDir = UP;
  elevator.currentFloor = 0;

  lcd.setCursor(0, 0);
  lcd.write(255);
  for (int i = 1; i < 16; i++){
    lcd.setCursor(i, 0);
    lcd.print("-");
  }

  Serial.println("Elevator FSM initialized");
  lcd.setCursor(0,1);
  lcd.print("Elevator Ready");

}

void loop() {

  /*
  EMERGENCY STOP CODE - General call 
  */
  if(digitalRead(BUTTONPIN) == LOW){ //switch the LOW to HIGH based on wiring of button 
    elevator.currentState = EMERGENCY_STOP; 
  }

  if(elevator.currentState == EMERGENCY_STOP){
    Serial.println("EMERGENCY STOP ACTIVATED");

    lcd.clear();
    lcd.setcursor(0,0);
    lcd.print("!!! EMERGENCY !!!");
    
    return; 
  }
  /*
    Floor request parsing
  */
  if (Serial.available() > 0){
    String inputCMD = Serial.readStringUntil('\n'); // reads the input string until \n is read
    inputCMD.trim(); // cuts the \r and \n from the string

    int spaceIndex = inputCMD.indexOf(' ');

    if (spaceIndex != -1){ // Ensures a space exist in the input
      int floorNum = inputCMD.substring(spaceIndex + 1).toInt(); // reads the left characters in the string and translates them into int

      String command = inputCMD.substring(0, spaceIndex); // reads the right characters in the string 

      if (floorNum < 1 || floorNum > 16) Serial.println("Invalid Floor");

      Serial.print("Word: "); Serial.println(command);
      Serial.print("Number: "); Serial.println(floorNum); 

    // reads the inputted request and determines direction desired
      if (command.equalsIgnoreCase("Up")){
        Serial.println("Place in up queue");

        insertUpRequest(floorNum);
      }

      else if (command.equalsIgnoreCase("Down")){
        Serial.println("Place in down queue");

        insertDownRequest(floorNum);
      }
      // reads the inputted request and determines which queue the desired floor should be placed in
      else if (command.equalsIgnoreCase("Floor")){
        if (elevator.currentFloor < floorNum){
          insertUpRequest(floorNum);
        }

        else {
          insertDownRequest(floorNum);
        }

      }

      else {
        Serial.println("Invalid request");
      }

    }
    
  }

  /*
    UP and down queue switching logic
  */
  int targetFloor = -1;
  // Pops all the requests in onen direction and switches direction once there are no requests
  if (elevator.currentDir == UP){
    elevator.currentState = MOVING_UP;
    if (upCount > 0){
      targetFloor = popUpRequest();
      moveToFloor(targetFloor);
    }

    else if (downCount > 0){
      elevator.currentDir = DOWN;
      Serial.println("Switching to down queue");
    }
  }

  else if (elevator.currentDir == DOWN){
    elevator.currentState = MOVING_DOWN;
    if (downCount > 0){
      targetFloor = popDownRequest();
      moveToFloor(targetFloor);
    }

    else if (upCount > 0){
      elevator.currentDir = UP;
      Serial.println("Switching to up queue");
    }
  }


}
