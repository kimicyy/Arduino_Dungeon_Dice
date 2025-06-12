#include "funshield.h"

constexpr int DISPLAY_POSITIONS = 4;
constexpr byte DISPLAY_CLEAR_MASK = 0b11111111;
constexpr byte DISPLAY_ALL_POSITIONS = (1 << DISPLAY_POSITIONS) - 1;
constexpr byte DIGIT_GLYPHS[] = { 0b11000000, 0b11111001, 0b10100100, 0b10110000, 0b10011001, 0b10010010, 0b10000010, 0b11111000, 0b10000000, 0b10010000 };
constexpr int TEXTNUMERIC_DISPLAY_INITIAL_VALUE = 0;

// map of letter glyphs
constexpr byte LETTER_GLYPH[] {
  0b10001000,   // A
  0b10000011,   // b
  0b11000110,   // C
  0b10100001,   // d
  0b10000110,   // E
  0b10001110,   // F
  0b10000010,   // G
  0b10001001,   // H
  0b11111001,   // I
  0b11100001,   // J
  0b10000101,   // K
  0b11000111,   // L
  0b11001000,   // M
  0b10101011,   // n
  0b10100011,   // o
  0b10001100,   // P
  0b10011000,   // q
  0b10101111,   // r
  0b10010010,   // S
  0b10000111,   // t
  0b11000001,   // U
  0b11100011,   // v
  0b10000001,   // W
  0b10110110,   // ksi
  0b10010001,   // Y
  0b10100100,   // Z
};
constexpr byte EMPTY_GLYPH = 0b11111111;
constexpr byte UNKNOWN_GLYPH = 0b10101010;
constexpr int BASE = 10;
constexpr int BUTTON_PINS[] = { button1_pin, button2_pin, button3_pin };
constexpr int BUTTON_COUNT = sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]);
constexpr int BUTTON_INDEX_GENERATE = 0;
constexpr int BUTTON_INDEX_THROW_CHANGE = 1;
constexpr int BUTTON_INDEX_TYPE_CHANGE = 2;
constexpr unsigned long DEBOUNCING_TIME = 10;
constexpr unsigned long LONGPRESS_ACTIVATION = 300;
constexpr unsigned long LONGPRESS_PERIOD = 100;
constexpr int DICES[] = { 4, 6, 8, 10, 12, 20, 100 };
constexpr int DICE_COUNT = sizeof(DICES) / sizeof(DICES[0]);
constexpr int DICE_MIN_THROW = 1;
constexpr int DICE_MAX_THROW = 9;
constexpr int DICE_TEXT_THROW_INDEX = 0;
constexpr int DICE_TEXT_INDEX = 1;
constexpr int DICE_TEXT_TYPE_INDEX_0 = 2;
constexpr int DICE_TEXT_TYPE_INDEX_1 = 3;
constexpr char DICE_TEXT = 'd';
// pick a prime number that is not too close to the power of 2 and also not too large (since button 1 pressed time won't be too long in general) for myHash function
constexpr int HASH_M = 383;

int power(int power, int BASE){
  int number = 1;
  for (int i = 0; i < power; i++){
    number *= BASE;
  }
  return number;
}

// Timer ok
class Timer {
  private:

    unsigned long previousTime;
  
  public:
    
    void initialize(unsigned long initialTime) {
      previousTime = initialTime;
    }

    unsigned long getElapsedTime(unsigned long currentTime) {
      if (currentTime < previousTime) {
        return (~(unsigned long)0 - previousTime + 1 + currentTime);
      }
      return currentTime - previousTime;
    }

    bool triggerNewEvent(unsigned long currentTime, unsigned long duration) {
      unsigned long elapsedTime = getElapsedTime(currentTime);
      if (elapsedTime >= duration) {
        if (currentTime < previousTime) {
          previousTime = duration - (~(unsigned long)0 - previousTime);
        } else {
          previousTime += duration;
        }
        return true;
      }
      return false;
    }

    void reset(unsigned long resetTime) {
      previousTime = resetTime;
    }
};

// Button ok
class Button{
  private:

    int buttonNumber;
    bool currentState;
    Timer timerLongPress;
    Timer timerDebouncing;
    Timer timerPressed;
    unsigned long pressedTime;
    unsigned long pressStartTimeStamp;
    unsigned long pressEndTimeStamp;
    bool longPressed;
    bool newState;
    bool pressAndReleaseEvent;

  public:

    void initialize(int number) {
      unsigned long initialTime = millis();
      buttonNumber = number;
      currentState = false;
      pinMode(BUTTON_PINS[buttonNumber], INPUT);
      longPressed = false;
      timerDebouncing.initialize(initialTime);
      timerPressed.initialize(initialTime);
    }

    bool pressed(){
      return (digitalRead(BUTTON_PINS[buttonNumber]) == LOW);
    }

    bool triggeredWithLongPress(unsigned long queryTime){  
      if (timerDebouncing.triggerNewEvent(queryTime, DEBOUNCING_TIME)){
        newState = pressed();
      }
      else{
        newState = currentState;
      }
      if (currentState == false && newState == true){
        currentState = newState;
        timerLongPress.initialize(queryTime);
        return true;
      }
      if (currentState == true && newState == true){
        if (!longPressed && timerLongPress.getElapsedTime(queryTime) >= LONGPRESS_ACTIVATION){
          timerLongPress.reset(queryTime);
          longPressed = true;
          return true;
        }
        if (longPressed && timerLongPress.triggerNewEvent(queryTime, LONGPRESS_PERIOD)){
          return true;
        }
      }
      if (currentState == true && newState == false){
        longPressed = false;
      }
      currentState = newState;
      return false;
    }

    bool triggered(unsigned long queryTime){
      // detect button press started  
      if (!currentState && pressed()){
        timerPressed.reset(queryTime);
        pressStartTimeStamp = queryTime;
        pressedTime = 0;
        setPressAndReleaseEvent(false);
      }
      // detect button released
      else if (currentState && !pressed()){
        setPressAndReleaseEvent(true);
        pressEndTimeStamp = queryTime;
        pressedTime = timerPressed.getElapsedTime(queryTime);
      }

      currentState = pressed();
      
      return currentState;
    }

    unsigned long getPressedTime(){
      return pressedTime;
    }

    unsigned long getPressStartTimeStamp(){
      return pressStartTimeStamp;
    }

    unsigned long getPressEndTimeStamp(){
      return pressEndTimeStamp;
    }

    bool getPressAndReleaseEvent(){
      return pressAndReleaseEvent;
    }

    void setPressAndReleaseEvent(bool newButtonState){
      pressAndReleaseEvent = newButtonState;
    }

};

// Display ok
class Display {
  public:

    void initialize() {
      pinMode(latch_pin, OUTPUT);
      pinMode(data_pin, OUTPUT);
      pinMode(clock_pin, OUTPUT);
      showGlyph(DISPLAY_CLEAR_MASK, DISPLAY_ALL_POSITIONS);
    }

    void showGlyph(byte glyph, byte position) {
      digitalWrite(latch_pin, LOW);
      shiftOut(data_pin, clock_pin, MSBFIRST, glyph);
      shiftOut(data_pin, clock_pin, MSBFIRST, position);
      digitalWrite(latch_pin, HIGH);
    }

    void showDigit(int digit, int position) {
      byte positionByte = 1 << (DISPLAY_POSITIONS - position - 1);
      showGlyph(DIGIT_GLYPHS[digit], positionByte);
    }

    void showSpace(int position){
      byte positionByte = 1 << (DISPLAY_POSITIONS - position - 1);
      showGlyph(EMPTY_GLYPH, positionByte);       
    }

    void showLetter(char letter, int position){
      byte positionByte = 1 << (DISPLAY_POSITIONS - position - 1);
      showGlyph(isUpperCase(letter) ? LETTER_GLYPH[letter - 'A'] : LETTER_GLYPH[letter - 'a'], positionByte);
    }

    void showUnknown(int position){
      byte positionByte = 1 << (DISPLAY_POSITIONS - position - 1);
      showGlyph(UNKNOWN_GLYPH, positionByte);
    }

    void showChar(char symbol, int position){
      if (isAlpha(symbol)){
        showLetter(symbol, position);
      }
      else if (isDigit(symbol)){
        showDigit(symbol - '0', position);
      }
      else if (isSpace(symbol)){
        showSpace(position);
      }
      else{
        showUnknown(position);
      }
    }
};

// TextNumericDisplay ok
class TextNumericDisplay : public Display{
  private:
    int position;
    int number;
    bool displayActive;
    bool isNumeric;
    int order;
    int highestPosition;
    char textToDisplay[DISPLAY_POSITIONS];

  public:
    void initialize(){
      Display::initialize();
      number = 0;
      position = 0;
      displayActive = false;
      order = power(position, BASE);
    }

    void setNumber(int numberToSet){
      number = numberToSet;
      displayActive = true;
      isNumeric = true;
      findHighestPosition();
    }

    void setString(const char sourceString[]){
      displayActive = true;
      isNumeric = false;
      for (int i = 0; i < DISPLAY_POSITIONS; ++i){
        textToDisplay[i] = sourceString[i];
      }
    }

    void findHighestPosition(){
      int value = number;
      highestPosition = 0;
      while ((value / BASE) > 0){
        highestPosition++;
        value /= BASE;
      }
    }

    void deactivate(){
      displayActive = false;
    }

    void numericUpdate(){
      if (!displayActive){
        return;
      }
      int digit = (number / order) % BASE;
      if (position <= highestPosition){
        showDigit(digit, position);
      }
      position = (position + 1) % DISPLAY_POSITIONS;
      order = (position != 0) ? order * BASE : 1;
    }

    void textUpdate(){
      if (!displayActive){
        return;
      }
      showChar(textToDisplay[DISPLAY_POSITIONS - 1 - position], position);
      (position == DISPLAY_POSITIONS - 1) ? position = 0 : position++;
    }

    void update(){
      isNumeric ? numericUpdate() : textUpdate();
    }

};

class Randomizer{
  private:

    int min;
    int max;

  public:

    void initialize(int maxRange){
      min = 1;
      max = maxRange;
    }

    void changeMax(int newMax){
      max = newMax;
    }

    int myHash(unsigned long number){
      return number % HASH_M;
    }

    int getRandomNumber(unsigned long input){
      return myHash((input + millis()) % max + 1);
    }
};

enum class DiceState { NORMAL, CONFIGURATION };

// Dice ok
class Dice{
  private:

    DiceState currentState;
    int typeIndex;
    Randomizer randomizer;
    char configString[DISPLAY_POSITIONS];
    bool configStringChanged;
    int throws;
    int result;
  
  public:

    void initialize(unsigned long initialTime){
      currentState = DiceState::NORMAL;
      typeIndex = 0;
      randomizer.initialize(DICES[typeIndex]);
      configString[DICE_TEXT_INDEX] = DICE_TEXT;
      configStringChanged = false;
      throws = DICE_MIN_THROW;
      result = 0;
    }

    void setState(DiceState newState){
      currentState = newState;
    }

    DiceState getState(){
      return currentState;
    }

    void generateResult(unsigned long* factorsPtr, int factors){
      result = 0;
      for (int i = 0; i < throws; i++){
        result += randomizer.getRandomNumber(factorsPtr[(i % factors)]);
      }
    }

    void throwUpdate(){
      configStringChanged = true;
      (throws == DICE_MAX_THROW) ? throws = DICE_MIN_THROW : throws++;
    }

    void typeUpdate(){
      configStringChanged = true;
      (typeIndex == DICE_COUNT - 1) ? typeIndex = 0 : typeIndex++;
      randomizer.changeMax(DICES[typeIndex]);
    }

    // ok
    void setConfigString(){
      configString[DICE_TEXT_THROW_INDEX] = throws + '0';
      if (typeIndex == DICE_COUNT - 1){
        configString[DICE_TEXT_TYPE_INDEX_0] = '0';
        configString[DICE_TEXT_TYPE_INDEX_1] = '0';
      }
      else if (DICES[typeIndex] >= BASE){
        configString[DICE_TEXT_TYPE_INDEX_0] = DICES[typeIndex] / BASE + '0';
        configString[DICE_TEXT_TYPE_INDEX_1] = DICES[typeIndex] % BASE + '0';
      }
      else{
        configString[DICE_TEXT_TYPE_INDEX_0] = DICES[typeIndex] + '0';
        configString[DICE_TEXT_TYPE_INDEX_1] = ' ';
      }
    }

    // ok
    char* getConfigString(){
      if (configStringChanged){
        setConfigString();
        configStringChanged = false;
      }
      return configString;
    }

    int getResult(){
      return result;
    }

    int getType(){
      return DICES[typeIndex];
    }

    int getThrows(){
      return throws;
    }
};

Dice dice;
Button buttons[BUTTON_COUNT];
TextNumericDisplay display;

void buttonGenerateCheck(unsigned long queryTime){
  if (buttons[BUTTON_INDEX_GENERATE].triggered(queryTime)){
    if (dice.getState() != DiceState::NORMAL){
      dice.setState(DiceState::NORMAL);
    }
    // just to show some unrelated random numbers within the range of the given type and throws while button 1 pressed
    display.setNumber(random(1, dice.getType() * dice.getThrows() + 1));
  }
  else{
    if (buttons[BUTTON_INDEX_GENERATE].getPressAndReleaseEvent()){
      buttons[BUTTON_INDEX_GENERATE].setPressAndReleaseEvent(false);
      unsigned long factors[] = { buttons[BUTTON_INDEX_GENERATE].getPressedTime(), buttons[BUTTON_INDEX_GENERATE].getPressStartTimeStamp(), 
                                  buttons[BUTTON_INDEX_GENERATE].getPressEndTimeStamp() };
      dice.generateResult(factors, sizeof(factors) / sizeof(factors[0]));
      display.setNumber(dice.getResult());
    }
  }
}

void checkDiceNormalToConfig(){
  if (dice.getState() != DiceState::CONFIGURATION){
      dice.setState(DiceState::CONFIGURATION);
  }
}

void buttonThrowChangeCheck(unsigned long queryTime){
  if (buttons[BUTTON_INDEX_THROW_CHANGE].triggeredWithLongPress(queryTime)){
    checkDiceNormalToConfig();
    dice.throwUpdate(); 
  }  
}

void buttonTypeChangeCheck(unsigned long queryTime){
  if (buttons[BUTTON_INDEX_TYPE_CHANGE].triggeredWithLongPress(queryTime)){
    checkDiceNormalToConfig();
    dice.typeUpdate(); 
  }  
}

void setup() {
  unsigned long initialTime = millis();
  for (int i = 0; i < BUTTON_COUNT; i++){
        buttons[i].initialize(i);
  }
  display.initialize();
  display.setNumber(TEXTNUMERIC_DISPLAY_INITIAL_VALUE);
  dice.initialize(initialTime);
}

void loop() {
  unsigned long currentTime = millis();
  
  buttonGenerateCheck(currentTime);
  buttonThrowChangeCheck(currentTime);
  buttonTypeChangeCheck(currentTime);

  if (dice.getState() == DiceState::CONFIGURATION){
    display.setString(dice.getConfigString());
  }

  display.update();

}
