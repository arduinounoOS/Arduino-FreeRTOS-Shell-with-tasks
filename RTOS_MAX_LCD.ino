#include <Arduino_FreeRTOS.h>
#include <EEPROM.h>
#include <queue.h>
#include <string.h>  // Include for string operations like strcmp, strcpy
#include <LiquidCrystal.h>


// Constants
#define BUFFER_SIZE 64
#define LED_PIN 13
#define MAX_FILENAME_SIZE 9  // Allow room for null terminator
#define MAX_FILE_SIZE 24
#define NUM_FILES 5

// EEPROM addresses
#define EEPROM_START_ADDR 0
#define FILE_SLOT_SIZE (MAX_FILENAME_SIZE - 1 + MAX_FILE_SIZE)

#define CPU_FREQUENCY F_CPU
#define FREE_RAM_SIZE freeRam()

LiquidCrystal lcd(8,9, 4, 5, 6, 7);
int lcd_key = 0;
int adc_key_in = 0;
int H = 0;
int V = 0;
int btn = 0;
int pdel = 0;
int fdel = 0;
String curs = "X";
#define btnRIGHT 0
#define btnUP 1
#define btnDOWN 2
#define btnLEFT 3
#define btnSELECT 4
#define btnNONE 5
byte heart[8] = {
  B00000,
  B01010,
  B10101,
  B10001,
  B10001,
  B01010,
  B00100,
  B00000
};

// Global Variables
int blinkDelay = 500;
char currentFilename[MAX_FILENAME_SIZE];
bool textEntryMode = false;

unsigned long bootSeconds = 0; // Counter for seconds since boot

// Function declarations
void TaskShell(void *pvParameters);
void TaskBlink(void *pvParameters);
void TaskUptime(void *pvParameters);
void TaskLCD(void *pvParameters);
void processCommand(const char *command);
void writeFile(const char *filename, const char *text);
void deleteFile(const char *filename);
bool fileExists(const char *filename, int &index);
bool hasAvailableSlot();
void readFile(const char *filename);
void listFiles();
void formatFileSystem();
void cpuInfo();
// Task handles for monitoring
TaskHandle_t xHandleBlink = NULL;
TaskHandle_t xHandleUptime = NULL;
TaskHandle_t xHandleLCD = NULL;
TaskHandle_t xHandleSerialShell = NULL; // Add handle for SerialShellTask

// Task array
TaskStatus_t userTasks[4]; // Increase size to include SerialShellTask


int freeRam() {
#if defined(__AVR__)
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
#elif defined(ESP8266) || defined(ESP32)
  return ESP.getFreeHeap();
#else
  return -1; // If unknown system
#endif
}

// Command buffer
char commandBuffer[BUFFER_SIZE];
uint8_t commandIndex = 0;



// Hardware setup
void setup() {
  // Initialize Serial Monitor
  Serial.begin(9600);

  // Display boot message
  Serial.println("\nFreeRTOS Booted.");
  Serial.print("> ");
  
  // Initialize LED pin
  pinMode(LED_PIN, OUTPUT);
  
lcd.begin(16, 2); // start the library
lcd.clear();
lcd.setCursor(H,V); // h is 0-16,V is 0-1
lcd.print("X");
  lcd.createChar(5, heart);

  // Check if the config file exists
  int index;
  if (fileExists("config", index)) {
    // Read blink delay from the config file
    readConfigFile(index);
  } else {
    Serial.println("No config file.");
  }

  // Create tasks
  xTaskCreate(TaskShell, "Shell", 130, NULL, 1, &xHandleSerialShell);
  xTaskCreate(TaskBlink, "Blink", 46, NULL, 1, &xHandleBlink);
  xTaskCreate(TaskUptime, "Uptime", 46, NULL, 1, &xHandleUptime);
  xTaskCreate(TaskLCD, "LCD", 86, NULL, 1, &xHandleLCD);

  userTasks[0].xHandle = xHandleSerialShell;
  userTasks[1].xHandle = xHandleBlink;
  userTasks[2].xHandle = xHandleUptime;
  userTasks[3].xHandle = xHandleLCD; // Include SerialShellTask handle

  // Allow FreeRTOS to take over
  vTaskStartScheduler();
}

void TaskShell(void *pvParameters) {
  (void) pvParameters;
  
  for (;;) {
    if (Serial.available() > 0) {
      char inChar = (char)Serial.read();
      
      if (!(inChar == '\n' || inChar == '\r')) {
        Serial.print(inChar);
      }

      if (inChar == '\n' || inChar == '\r') {
        commandBuffer[commandIndex] = '\0';  // Null-terminate the string
        Serial.println();
        if (textEntryMode) {
          writeFile(currentFilename, commandBuffer);
          textEntryMode = false;
          Serial.print("> ");  // Return to main prompt
        } else {
          if (commandIndex > 0) {
            processCommand(commandBuffer);
          }
          Serial.print("> ");
        }
        commandIndex = 0;
      } else if (commandIndex < BUFFER_SIZE - 1) {
        commandBuffer[commandIndex++] = inChar;
      }
    }

    vTaskDelay(1);
  }
}

void TaskLCD(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
lcd_key = read_LCD_buttons();
curs = String("X"); 
if (lcd_key == btn) {
if (lcd_key != btnNONE) {
pdel++;
if (fdel == 0) {
if (pdel > 1000) {pdel = 0; fdel++;}
}
else {
curs = String("*");
if (pdel > 250) {pdel = 0;}
}
}
}
else {
btn = lcd_key;
pdel = 0;
fdel = 0;
}
if (pdel == 0) {
switch (lcd_key)
{
case btnRIGHT:
{
H = constrain(H + 1,0,15);
break;
}
case btnLEFT:
{
H = constrain(H -1,0,15);
break;
}
case btnUP:
{
V = constrain(V -1,0,1);
break;
}
case btnDOWN:
{
V = constrain(V +1,0,1);
break;
}
case btnSELECT:
{
curs = String("*");
break;
}
case btnNONE:
{
curs = String("X");
break;
}
}

lcd.clear();
lcd.setCursor(H,V); // h is 0-16,V is 0-1
if (lcd_key == btnSELECT) {lcd.write(5);}
else {lcd.print(curs);}
}
}
}

int read_LCD_buttons()
{
adc_key_in = analogRead(0); // read the value from the sensor
// my buttons when read are centered at these valies: 0, 144, 329, 504, 741
// we add approx 50 to those values and check to see if we are close
if (adc_key_in > 1000) return btnNONE; // We make this the 1st option for speed reasons since it will be the most likely result
if (adc_key_in < 50) return btnRIGHT;
if (adc_key_in < 195) return btnUP;
if (adc_key_in < 380) return btnDOWN;
if (adc_key_in < 555) return btnLEFT;
if (adc_key_in < 790) return btnSELECT;
return btnNONE; // when all others fail, return this...
}

void readConfigFile(int index) {
  int startAddr = EEPROM_START_ADDR + (index * FILE_SLOT_SIZE);
  char configContent[MAX_FILE_SIZE];
  for (int i = 0; i < MAX_FILE_SIZE; i++) {
    char c = EEPROM.read(startAddr + MAX_FILENAME_SIZE - 1 + i);
    if (c == '\0') break;
    configContent[i] = c;
  }
  configContent[MAX_FILE_SIZE - 1] = '\0';

  int configDelay = atoi(configContent);
  if (configDelay > 0) {
    blinkDelay = configDelay;
  } 
}

void TaskBlink(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    digitalWrite(LED_PIN, HIGH);
    vTaskDelay(blinkDelay / portTICK_PERIOD_MS);
    digitalWrite(LED_PIN, LOW);
    vTaskDelay(blinkDelay / portTICK_PERIOD_MS);
  }
}

void TaskUptime(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    bootSeconds++;
  }
}

void processCommand(const char *command) {
  if (strncmp(command, "sd ", 3) == 0) {
    int newDelay = atoi(command + 3);
    if (newDelay > 0) {
      blinkDelay = newDelay;
      Serial.print(": ");
      Serial.print(blinkDelay);
      Serial.println(" ms");
    } else {
      Serial.println("?");
    }
  }
  else if (strncmp(command, "wr ", 3) == 0) {
    const char *filename = command + 3;
    int index;
    if (fileExists(filename, index)) {
      Serial.println("Exists");
    } else if (!hasAvailableSlot()) {
      Serial.println("FS Full");
    } else {
      strncpy(currentFilename, filename, MAX_FILENAME_SIZE - 1);
      currentFilename[MAX_FILENAME_SIZE - 1] = '\0';  // Ensure null-terminated
      Serial.print(":");
      Serial.print(currentFilename);
      Serial.println(":");
      textEntryMode = true;
      Serial.print(">> ");
    }
  }
  else if (strncmp(command, "rd ", 3) == 0) {
    const char *filename = command + 3;
    readFile(filename);
  }
  else if (strncmp(command, "de ", 3) == 0) {
    const char *filename = command + 3;
    deleteFile(filename);
  }
  else if (strcmp(command, "li") == 0) {
    listFiles();
  }
  else if (strcmp(command, "ft") == 0) {
    formatFileSystem();
    Serial.println("FS fmtd");
  }
  else if (strcmp(command, "st") == 0) {
    cpuInfo();
  }
  else if (strcmp(command, "ut") == 0) {
    Serial.print("Seconds since boot: ");
    Serial.println(bootSeconds);
  }
  else if (strcmp(command, "?") == 0) {
    int index;
    if (fileExists("helpfile", index)) {
      readFile("helpfile");
    } else {
      Serial.println("No helpfile.");
    }
  } else {
    Serial.println("? - help");
  }
}

// Check if a file exists
bool fileExists(const char *filename, int &index) {
  for (int i = 0; i < NUM_FILES; i++) {
    int startAddr = EEPROM_START_ADDR + (i * FILE_SLOT_SIZE);
    char name[MAX_FILENAME_SIZE];
    for (int j = 0; j < MAX_FILENAME_SIZE - 1; j++) {
      name[j] = EEPROM.read(startAddr + j);
    }
    name[MAX_FILENAME_SIZE - 1] = '\0';
    if (strcmp(name, filename) == 0) {
      index = i;
      return true;
    }
  }
  return false;
}

// Check if there is an available slot for a new file
bool hasAvailableSlot() {
  for (int i = 0; i < NUM_FILES; i++) {
    int startAddr = EEPROM_START_ADDR + (i * FILE_SLOT_SIZE);
    if (EEPROM.read(startAddr) == '\0') {
      return true;
    }
  }
  return false;
}

// EEPROM file write
void writeFile(const char *filename, const char *text) {
  int index = -1;
  for (int i = 0; i < NUM_FILES; i++) {
    int startAddr = EEPROM_START_ADDR + (i * FILE_SLOT_SIZE);
    if (EEPROM.read(startAddr) == '\0') {
      index = i;
      break;
    }
  }

  if (index == -1) {
    Serial.println("No available slots for writing.");
    return;
  }

  Serial.print("Writing to file ");
  Serial.println(filename);
  int startAddr = EEPROM_START_ADDR + (index * FILE_SLOT_SIZE);
  for (int i = 0; i < MAX_FILENAME_SIZE - 1; i++) {
    EEPROM.write(startAddr + i, filename[i]);
  }
  for (int i = 0; i < MAX_FILE_SIZE; i++) {
    if (text[i] == '\0') {
      EEPROM.write(startAddr + MAX_FILENAME_SIZE - 1 + i, '\0');
      break;
    }
    EEPROM.write(startAddr + MAX_FILENAME_SIZE - 1 + i, text[i]);
  }
  Serial.println("Write complete.");
}

// EEPROM file delete
void deleteFile(const char *filename) {
  int index;
  if (!fileExists(filename, index)) {
    Serial.println("File not found.");
    return;
  }

  int startAddr = EEPROM_START_ADDR + (index * FILE_SLOT_SIZE);
  for (int i = 0; i < FILE_SLOT_SIZE; i++) {
    EEPROM.write(startAddr + i, '\0');
  }
  Serial.print(filename);
  Serial.println(" deleted.");
}

// EEPROM file read
void readFile(const char *filename) {
  int index = -1;
  if (!fileExists(filename, index)) {
    Serial.println("Not found.");
    return;
  }

  int startAddr = EEPROM_START_ADDR + (index * FILE_SLOT_SIZE);
  Serial.print(filename);
  Serial.println(" content:");
  for (int i = 0; i < MAX_FILE_SIZE; i++) {
    char c = EEPROM.read(startAddr + MAX_FILENAME_SIZE - 1 + i);
    if (c == '\0') break;
    Serial.print(c);
  }
  Serial.println();
}

// List files in EEPROM
void listFiles() {
  char filenames[NUM_FILES][MAX_FILENAME_SIZE];
  int fileCount = 0;

  // Read filenames into an array
  for (int i = 0; i < NUM_FILES; i++) {
    int startAddr = EEPROM_START_ADDR + (i * FILE_SLOT_SIZE);
    for (int j = 0; j < MAX_FILENAME_SIZE - 1; j++) {
      filenames[fileCount][j] = EEPROM.read(startAddr + j);
    }
    filenames[fileCount][MAX_FILENAME_SIZE - 1] = '\0';

    if (filenames[fileCount][0] != '\0') {
      // Include only non-empty filenames
      fileCount++;
    }
  }

  // Sort filenames array
  for (int i = 0; i < fileCount - 1; i++) {
    for (int j = i + 1; j < fileCount; j++) {
      if (strcmp(filenames[i], filenames[j]) > 0) {
        char temp[MAX_FILENAME_SIZE];
        strcpy(temp, filenames[i]);
        strcpy(filenames[i], filenames[j]);
        strcpy(filenames[j], temp);
      }
    }
  }

  // Output sorted filenames with inset
  for (int i = 0; i < fileCount; i++) {
    Serial.print(" ");
    Serial.println(filenames[i]);
  }
}

// Format the filesystem
void formatFileSystem() {
  for (int i = 0; i < NUM_FILES; i++) {
    int startAddr = EEPROM_START_ADDR + (i * FILE_SLOT_SIZE);
    for (int j = 0; j < FILE_SLOT_SIZE; j++) {
      EEPROM.write(startAddr + j, '\0');
    }
  }
}

// Show CPU information
void cpuInfo() {
  Serial.print("Hz: ");
  Serial.println(CPU_FREQUENCY); // Convert Hz to MHz
            Serial.println("Task\tHWM");
          for (int i = 0; i < 4; i++) {
            uint16_t highWaterMark = uxTaskGetStackHighWaterMark(userTasks[i].xHandle);
            Serial.print(i);
            Serial.print("\t");
            Serial.println(highWaterMark);
          }
}

// Main loop - unused in FreeRTOS
void loop() {
  // Do nothing
}