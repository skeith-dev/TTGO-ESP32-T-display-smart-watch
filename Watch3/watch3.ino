#define FS_NO_GLOBALS
#include <FS.h>
#include <Arduino.h>


#ifdef ESP32
  #include "SPIFFS.h" //ESP32 only
#endif


#include <JPEGDecoder.h>
#include "RTClib.h"
#include <TFT_eSPI.h> //hardware-specific library
#include <SPI.h>
#include "Button2.h";


#include <WiFi.h>
#include <HTTPClient.h>


#include <ArduinoJson.h>


#define BUTTON_PINRT  35  //pin number for main right button is 35
#define BUTTON_PINLE  0   //pin number for main left button is 0
#define BUTTON_PINRT2  27 //pin number for side right button is 27
#define BUTTON_PINLE2  13 //pin number for side left button is 13
#define bl 4  //pin number for (presumably) built-in blue led


//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//
//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//


Button2 bRt = Button2(BUTTON_PINRT);  //initialize right button
Button2 bLe = Button2(BUTTON_PINLE);  //initialize left button
Button2 bRt2 = Button2(BUTTON_PINRT2);  //initialize right side button
Button2 bLe2 = Button2(BUTTON_PINLE2);  //initialize left side button

TFT_eSPI tft = TFT_eSPI();  //invoke custom library
RTC_DS3231 rtc;             //initialize RTC module

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
float oldSecond;
float newSecond;

int oldMinute;
int newMinute;

int oldHour;
int newHour;

float oldTemp;
float newTemp;

bool newDay;

int screen; //SCREEN IS OF INT TYPE
int oldScreen;  //OLDSCREEN IS OF INT TYPE

unsigned long tUntilPressed;
unsigned long tAtPressed;

unsigned long sleepTimeActive;
unsigned long sleepTimeLazy;

float clockTime = 0.000;
int clockTimeInt;
int clockTimeMin;
int clockTimeHour;
int clockTimeOld;
int clockTimeMinOld;
int clockTimeHourOld;

int dailyChecklistSelection = -1;
int dailyCupsOfWater;
int dailyMealsEaten;
int dailyMysteryFed;
int dailyCupsOfWaterOld;
int dailyMealsEatenOld;
int dailyMysteryFedOld;

bool rtBeenPressed; //boolean for whether or not the right button has been pressed
bool leBeenPressed; //boolean for whether or not the left button has been pressed

int charge; //CHARGE IS OF INT TYPE
float batteryVoltage;
int chargeOld;
float batteryVoltageOld = 0;

int batOx = 0; 
int batOy = 220;
int antiFlicker = 1;

File root;
File file;
String line;
char lineIndex;
int data[3];

float temp = 0;
float apparentTemp = 0;
float precipIntensity = 0;
String precipType = "";
float precipProbability = 0;
float humidity = 0;
float pressure = 0;
float windSpeed = 0;
float cloudCover = 0;
float uvIndex = 0;
String summary = "";
float tempOld = 0;
float apparentTempOld = 0;
float precipIntensityOld = 0;
String precipTypeOld = "";
float precipProbabilityOld = 0;
float humidityOld = 0;
float pressureOld = 0;
float windSpeedOld = 0;
float cloudCoverOld = 0;
float uvIndexOld = 0;
String summaryOld = "";
const int kNetworkTimeout = 30 * 1000;
const int kNetworkDelay = 2000;
DynamicJsonDocument doc(2048);


//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//
//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//


void setup() {

  Serial.begin(115200);

  pinMode(bl, OUTPUT);

  #ifndef ESP8266
    while (!Serial);  //wait for serial port to connect. Needed for native USB
  #endif

  if (!rtc.begin()) { //if serial port does not connect, abort
    Serial.flush();
    abort();
  }

  //IMPORTANT - sets RTC to adjust time when restarted
  //uncomment to initialize time, recomment thereafter to hold RTC time
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  
  tft.begin();
  tft.init();   

  if(!SPIFFS.begin()) {
    Serial.println("SPIFFS initialisation failed!");
    while (1) yield(); //stay here twiddling thumbs waiting
  }
  Serial.println("\r\nINITIALIZATION COMPLETE\n");

  tft.fillScreen(TFT_BLACK);
  drawJpeg("/wallPaper.jpg", 0 , 0);

  bRt.setReleasedHandler(releasedRt); //set button released handler for right button
  bRt.setClickHandler(clickRt); //set button clicked handler for right button
  bRt.setLongClickHandler(longClickRt); //set button long clicked handler for right button
  bRt.setDoubleClickHandler(doubleClickRt); //set button double clicked handler for right button
  bRt.setTripleClickHandler(tripleClickRt); //set button triple clicked handler for right button

  bLe.setReleasedHandler(releasedLe);  //set button released handler for left button
  bLe.setClickHandler(clickLe); //set button clicked handler for left button
  bLe.setLongClickHandler(longClickLe); //set button long clicked handler for left button
  bLe.setDoubleClickHandler(doubleClickLe); //set button double clicked handler for left button
  bLe.setTripleClickHandler(tripleClickLe); //set button triple clicked handler for left button

  bRt2.setClickHandler(clickRt2); //set button clicked handler for right side button
  
  bLe2.setClickHandler(clickLe2); //set button clicked handler for left side button

  int filesCount = listFiles();
  checkExistsAndCreate("/dailyChecklistData.txt");
  deleteDuplicateFiles(filesCount);

  readFile("/dailyChecklistData.txt");
  retrieveData("/dailyChecklistData.txt");

  if(compareDates()) {
    dailyCupsOfWater = data[0];
    dailyMealsEaten = data[1];
    dailyMysteryFed = data[2];
    dailyCupsOfWaterOld = data[0];
    dailyMealsEatenOld = data[1];
    dailyMysteryFedOld = data[2];
  } else {
    dailyCupsOfWater = 0;
    dailyMealsEaten = 0;
    dailyMysteryFed = 0;
    dailyCupsOfWaterOld = 0;
    dailyMealsEatenOld = 0;
    dailyMysteryFedOld = 0;
  }

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, 0); //set main right button as wakeup button for deep sleep
  
}

void loop() {
  
  oldScreen = screen; //set oldScreen variable to the current screen (of type int)

  //loop checks the button pin each time,
  //and will send serial if it is pressed
  bRt.loop(); 
  bLe.loop(); 
  bRt2.loop();  
  bLe2.loop();  

  //IMPORTANT - switch case for the current screen
  //this executes the necessary code for each watch face
  //depending on the current screen
  switch (screen) {
    
    case 0: //tellTime watch face
      tellTime();
      break;
    case 1: //stopWatch watch face
      stopwatch();
      break;
    case 2: //dailyCheckList watch face
      dailyChecklist();
      break;
    case 3: //weatherTracker watch face
      weatherTracker();
      break;
      
  }
  
  if(oldScreen != screen) { //if the old screen does not equal the current screen
    drawJpeg("/wallPaper.jpg", 0 , 0);  //reset the background image to the wallpaper jpeg
    sleepTimeActive = millis(); //set the sleepTimeActive variable to the current milliseconds count
  }
  
  battery();  //execute the battery function
 
}


//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//
//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//


void pressedRt(Button2& bRt) {
}

void releasedRt(Button2& bRt) {
}

void changedRt(Button2& bRt) {
}

void clickRt(Button2& bRt) {
  if(screen == 3) {
    connectToWifi("12thirty", "sweetlawn766");
    retrieveWeatherData();
  }
  if(screen == 2) {
    switch (dailyChecklistSelection) {
      case -1:
        saveDataToFile("/dailyChecklistData.txt");
        break;
      case 0:
        dailyCupsOfWater++;
        break;
      case 1:
        dailyMealsEaten++;
        break;
      case 2:
        dailyMysteryFed++;
        break;
    }
  }
  if(screen == 1) {
    rtBeenPressed = true;
  }
  if(screen == 0) {
    sleepTimeActive = millis();
  }
}

void longClickRt(Button2& bRt) {
}

void doubleClickRt(Button2& bRt) { 
  if(screen == 2 && dailyChecklistSelection < 2) {
    dailyChecklistSelection++;
  }
  if(screen == 0) {
    sleepTimeActive = millis();
  }
}

void tripleClickRt(Button2& bRt) {
}

void tapRt(Button2& bRt) {
}


void pressedLe(Button2& bLe) {
}

void releasedLe(Button2& bLe) {
}

void changedLe(Button2& bLe) {
}

void clickLe(Button2& bLe) {
  if(screen == 2) {
    switch (dailyChecklistSelection) {
      case 0:
        if(dailyCupsOfWater > 0) {
          dailyCupsOfWater--;
        }
        break;
      case 1:
        if(dailyMealsEaten > 0) {
          dailyMealsEaten--;
        }
        break;
      case 2:
        if(dailyMysteryFed > 0) {
          dailyMysteryFed--;
        }
        break;
    }
  }
  if(screen == 1) {
    leBeenPressed = true;
  }
  if(screen == 0) {
    sleepTimeActive = millis();
  }
}

void longClickLe(Button2& bLe) {
}

void doubleClickLe(Button2& bLe) {
  if(screen == 2 && dailyChecklistSelection > -1) {
    dailyChecklistSelection--;
  }
  if(screen == 0) {
    sleepTimeActive = millis();
  }
}

void tripleClickLe(Button2& bLe) {
}

void tapLe(Button2& bLe) {
}

void clickRt2(Button2& bRt2) {
  if(screen < 3) {
    screen++;
  }
  if(screen == 0) {
    sleepTimeActive = millis();
  }
}

void clickLe2(Button2& bLe2) {
  if(screen > 0) {
     screen--;
  }
  if(screen == 0) {
    sleepTimeActive = millis();
  }
}


//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//
//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//


void connectToWifi(const char* wifiSSID, const char* wifiPassword) {

  Serial.print("CONNECTING WIFI: ");
  Serial.println(wifiSSID);
  int timeStart = millis();
  int timeEnd = millis();
  WiFi.begin(wifiSSID, wifiPassword);

  String fileName;
  int i = 0;
  
  while(WiFi.status() != WL_CONNECTED && timeEnd - timeStart < 10000) {
    Serial.print(".");
    delay(500);

    fileName = "/progressWheel";
    fileName += (i % 4);
    fileName += ".jpg";
    drawJpeg(fileName.c_str(), 110 , 215);
    
    timeEnd = millis();
    i++;
  }
  
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWIFI CONNECTED");
    Serial.print("IP ADDRESS: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WIFI CONNECTION FAILED");
  }

  
}


//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//
//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//


//return the minimum of two values a and b
#define minimum(a,b)     (((a) < (b)) ? (a) : (b))

int abs(int c) {
  return c * ((c > 0) - (c < 0));
}

//====================================================================================
//   Opens the image file and prime the Jpeg decoder
//====================================================================================
void drawJpeg(const char *filename, int xpos, int ypos) {

  // Open the named file (the Jpeg decoder library will close it after rendering image)
  fs::File jpegFile = SPIFFS.open( filename, "r");    // File handle reference for SPIFFS
  //  File jpegFile = SD.open( filename, FILE_READ);  // or, file handle reference for SD library

  //ESP32 always seems to return 1 for jpegFile so this null trap does not work
  if ( !jpegFile ) {
    Serial.print("ERROR: File \""); Serial.print(filename); Serial.println ("\" not found!");
    return;
  }

  // Use one of the three following methods to initialise the decoder,
  // the filename can be a String or character array type:

  //boolean decoded = JpegDec.decodeFsFile(jpegFile); // Pass a SPIFFS file handle to the decoder,
  //boolean decoded = JpegDec.decodeSdFile(jpegFile); // or pass the SD file handle to the decoder,
  boolean decoded = JpegDec.decodeFsFile(filename);  // or pass the filename (leading / distinguishes SPIFFS files)

  if (decoded) {
    // render the image onto the screen at given coordinates
    jpegRender(xpos, ypos);
  }
  else {
    Serial.println("Jpeg file format not supported!");
  }
}

//====================================================================================
//   Decode and render the Jpeg image onto the TFT screen
//====================================================================================
void jpegRender(int xpos, int ypos) {

  // retrieve infomration about the image
  uint16_t  *pImg;
  int16_t mcu_w = JpegDec.MCUWidth;
  int16_t mcu_h = JpegDec.MCUHeight;
  int32_t max_x = JpegDec.width;
  int32_t max_y = JpegDec.height;

  // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
  // Typically these MCUs are 16x16 pixel blocks
  // Determine the width and height of the right and bottom edge image blocks
  int32_t min_w = minimum(mcu_w, max_x % mcu_w);
  int32_t min_h = minimum(mcu_h, max_y % mcu_h);

  // save the current image block size
  int32_t win_w = mcu_w;
  int32_t win_h = mcu_h;

  // record the current time so we can measure how long it takes to draw an image
  uint32_t drawTime = millis();

  // save the coordinate of the right and bottom edges to assist image cropping
  // to the screen size
  max_x += xpos;
  max_y += ypos;

  // read each MCU block until there are no more
  while ( JpegDec.readSwappedBytes()) { // Swapped byte order read

    // save a pointer to the image block
    pImg = JpegDec.pImage;

    // calculate where the image block should be drawn on the screen
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;  // Calculate coordinates of top left corner of current MCU
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    // check if the image block size needs to be changed for the right edge
    if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
    else win_w = min_w;

    // check if the image block size needs to be changed for the bottom edge
    if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
    else win_h = min_h;

    // copy pixels into a contiguous block
    if (win_w != mcu_w)
    {
      for (int h = 1; h < win_h-1; h++)
      {
        memcpy(pImg + h * win_w, pImg + (h + 1) * mcu_w, win_w << 1);
      }
    }

    // draw image MCU block only if it will fit on the screen
    if ( mcu_x < tft.width() && mcu_y < tft.height())
    {
      // Now push the image block to the screen
      tft.pushImage(mcu_x, mcu_y, win_w, win_h, pImg);
    }

    else if ( ( mcu_y + win_h) >= tft.height()) JpegDec.abort();

  }

  // calculate how long it took to draw the image
  drawTime = millis() - drawTime; // Calculate the time it took

}


//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//
//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//


//====================================================================================
//                 Print a SPIFFS directory list (root directory)
//====================================================================================

#ifdef ESP32

int listFiles() {

  int count = 0;
  
  if(!SPIFFS.begin(true)) {
    Serial.println("Error: mounting SPIFFS");
  }

  root = SPIFFS.open("/");
  file = root.openNextFile();

  while(file) {
    Serial.print("File: ");
    Serial.println(file.name());
    count++;
    
    file = root.openNextFile();
  }
  Serial.println();
  
  return count;
  
}

void checkExistsAndCreate(char fileName[]) {

  if(!SPIFFS.begin(true)) {
    Serial.println("Error: mounting SPIFFS");
    return;
  }

  file = SPIFFS.open(fileName);

  if(file) {
    Serial.println("FILE EXISTS\n");
  } else {
    Serial.println("FILE DOES NOT EXIST\n");
  }
  
}

void deleteDuplicateFiles(int count) {

  int i, j;
  String fileNames[count];

  root = SPIFFS.open("/");
  file = root.openNextFile();

  for(i = 0; i < count; i++) {
    fileNames[i] = file.name();
    file = root.openNextFile();
  }

  for(i = 0; i < count - 1; i++) {

    for(j = i + 1; j < count; j++) {

      if(fileNames[i].equals(fileNames[j])) {
        Serial.print(fileNames[j]);
        Serial.println(" REMOVED\n");
        
        SPIFFS.remove(fileNames[j]);
      }
      
    }
    
  }
  
}

void saveDataToFile(char fileName[]) {

  file = SPIFFS.open(fileName, FILE_APPEND);

  file.print(rtc.now().month());
  file.print("/");
  file.print(rtc.now().day());
  file.print("  ");
  file.print(dailyCupsOfWater);
  file.print("  ");
  file.print(dailyMealsEaten);
  file.print("  ");
  file.print(dailyMysteryFed);
  file.print("\n");

  file.close();
  
}

void readFile(char fileName[]) {

  file = SPIFFS.open(fileName);

  while(file.available()) {
    Serial.write(file.read());
  }
  Serial.println();

  file.close();
  
}

void retrieveData(char fileName[]) {

  int length = 0;

  file = SPIFFS.open(fileName);

  lineIndex = (char) file.read();
  while(file.available()) {

    if(lineIndex == '\n') {
      line = "";
      length = 0;
    } else {
      line += lineIndex;
      length++;
    }
    lineIndex = (char) file.read();

  }

  file.close();

  Serial.print("LINE: ");
  Serial.print(line);
  Serial.println();

  int index = 0;
  int i;
  for(i = 1; i < length; i++) {

    if(line[i] != ' ' && line[i - 1] == ' ') {
      data[index] = line[i] - '0';
      index++;
    }
    
  }

  Serial.print("DATA: [ ");
  for(i = 0; i < 3; i++) {
    Serial.print(data[i]);
    Serial.print(" ");
  }
  Serial.println("]\n");
  
}

boolean compareDates() {

  String dataDate = "";
  String currentDate = "";

  int i = 0;
  while(!line.equals("") && line[i] != ' ') {
    dataDate += line[i];
    i++;
  }

  currentDate += rtc.now().month();
  currentDate += "/";
  currentDate += rtc.now().day();

  if(dataDate.equals(currentDate)) {
    Serial.print("DATES EQUAL: ");
    Serial.print(dataDate);
    Serial.print(" == ");
    Serial.print(currentDate);
    Serial.println();
    return true;
  } else {
    Serial.print("DATES NOT EQUAL: ");
    Serial.print(dataDate);
    Serial.print(" != ");
    Serial.println(currentDate);
    Serial.println();
    return false;
  }
  
}

#endif


//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//
//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//*****//


void tellTime() {

  //loop checks the button pin each time,
  //and will send serial if it is pressed
  bRt.loop();
  bLe.loop();
  
  oldSecond = newSecond; //old time is time after loop
  oldMinute = newMinute;
  oldHour = newHour;
  oldTemp = newTemp;
  DateTime now = rtc.now();
  newSecond = now.second();// newTime is assigned to current second
  newMinute = now.minute();
  newHour = now.hour();
  newTemp = rtc.getTemperature();

  //print the day of the week
  tft.setTextSize(2);
  tft.setCursor(10, 5);
  tft.print(daysOfTheWeek[now.dayOfTheWeek()]);
  tft.setTextColor(TFT_WHITE); 
  tft.setRotation(0);

  //print the hour
  tft.setCursor(10, 30);
  tft.setTextSize(4);
  if (now.hour() < 10) {
    tft.print("0");
  }
  tft.print(now.hour());
  tft.print(":");

  //print the minute
  if (now.minute() < 10) {
    tft.print("0");
  }
  tft.print(now.minute());

  //print the second
  tft.setCursor(10, 80);
  tft.setTextSize(3);
  if (now.second() < 10) {
    tft.print("0");
  }
  tft.print(now.second());

  //print the temperature
  tft.setTextSize(2);
  tft.setCursor(72, 85);
  tft.print(rtc.getTemperature());

  //draw the "squares" that appear for incrementing digits
  if (oldSecond != newSecond) {
    //tft.fillRect(10, 70, 35, 22, TFT_BLACK);
    drawJpeg("/secondsSquare.jpg", 10 , 80);
  }
  if (oldMinute != newMinute) {
    //tft.fillRect(78, 20, 50, 30, TFT_BLACK);
    drawJpeg("/minutesSquare.jpg", 78 , 30);
  }
  if(oldHour != newHour) {
    drawJpeg("/wallPaper.jpg", 0 , 0);
  }
  if(oldTemp != newTemp){
    //tft.fillRect(72, 85, 60, 16, TFT_GREEN);
    drawJpeg("/tempSquare.jpg", 72 , 85);
  }
  
  sleep();  //execute sleep function, which checks the idle time and sleeps accordingly

}

void drawBattery() {

  tft.drawLine(batOx, batOy, batOx, batOy+17, TFT_WHITE);
  tft.drawLine(batOx+1, batOy, batOx+1, batOy+17, TFT_WHITE);
  
  tft.drawLine(batOx, batOy, batOx+31, batOy, TFT_WHITE);
  tft.drawLine(batOx, batOy+1, batOx+31, batOy+1, TFT_WHITE);
  
  tft.drawLine(batOx+30, batOy, batOx+30, batOy+5, TFT_WHITE);
  tft.drawLine(batOx+31, batOy, batOx+31, batOy+5, TFT_WHITE);

  tft.drawLine(batOx, batOy+16, batOx+31, batOy+16, TFT_WHITE);
  tft.drawLine(batOx, batOy+17, batOx+31, batOy+17, TFT_WHITE);
  
  tft.drawLine(batOx+30, batOy+17, batOx+30, batOy+12, TFT_WHITE);
  tft.drawLine(batOx+31, batOy+17, batOx+31, batOy+12, TFT_WHITE);

  tft.drawLine(batOx+32, batOy+4, batOx+32, batOy+5, TFT_WHITE);
  tft.drawLine(batOx+32, batOy+12, batOx+32, batOy+13, TFT_WHITE);
  
  tft.drawLine(batOx+33, batOy+4, batOx+33, batOy+13, TFT_WHITE);
  tft.drawLine(batOx+34, batOy+4, batOx+34, batOy+13, TFT_WHITE);
  
}

void battery() {
  
  chargeOld = charge; //set chargeOld variable to the current screen (of type int)
  batteryVoltage = ( (float) analogRead(34) / 4095.0 ) * 2.0 * 3.3 * (1100 / 1000.0); //calculate battery voltage
  
  if(abs(batteryVoltageOld - batteryVoltage) > 0.2) {
    Serial.print("Battery voltage: ");
    Serial.println(batteryVoltage);

    batteryVoltageOld = batteryVoltage;
  }
  
  if(batteryVoltage > 3.91 && batteryVoltage < 4.35 || antiFlicker == 1) {  //if the battery voltage is between the range of 2/3 - 3/3 capacity
    
    charge = 100;
    antiFlicker = 1;
    
    drawBattery();

    tft.fillRect(batOx+4, batOy+3, 6, 12, TFT_WHITE); //fill battery icon for 1/3 full
    tft.fillRect(batOx+12, batOy+3, 6, 12, TFT_WHITE);  //fill battery icon for 2/3 full
    tft.fillRect(batOx+20, batOy+3, 6, 12, TFT_WHITE);  //fill battery icon for 1/3 full
  
  } else if(batteryVoltage < 3.91 && batteryVoltage > 3.78 || antiFlicker == 2) { //if the battery voltage is between the range of 1/3 - 2/3 capacity
    
    charge = 66;
    antiFlicker = 2;
    
    drawBattery();

    tft.fillRect(batOx+4, batOy+3, 6, 12, TFT_WHITE); //fill battery icon for 1/3 full
    tft.fillRect(batOx+12, batOy+3, 6, 12, TFT_WHITE); //fill battery icon for 2/3 full
  
  } else if(batteryVoltage < 3.78 && batteryVoltage || antiFlicker == 3 ) { //if the battery voltage is between the range of 0/3 - 1/3 capacity
    
    charge = 33;
    antiFlicker = 3;
    
    drawBattery();

    tft.fillRect(batOx+4, batOy+3, 6, 12, TFT_WHITE); //fill battery icon for 1/3 full
    
  }

  if(batteryVoltage > 4.5) { //if the battery voltage is greater than 4.35 (seemingly arbitrary value)
      tft.setTextSize(1); //set the text size
      tft.setCursor(0, 230);  //set the cursor
      tft.print("Connected"); //set the text to "Connected"
      charge = 101; //set charge int value to 101
      antiFlicker = 0;
  }
  
  if(chargeOld != charge) { //if chargeOld value is not equal to to the current charge value (of type int)
    drawJpeg("/wallPaper.jpg", 0 , 0); //reset the background image to the wallpaper jpeg
  }

}

void sleep() {

  //loop checks the button pin each time,
  //and will send serial if it is pressed
  bRt.loop();
  bLe.loop(); 

   //set the idle time to the current time in milliseconds minus the time
   //at which a button was last pressed
    sleepTimeLazy = (millis() - sleepTimeActive);

    //if the idle time is greater than 30 seconds
    if(sleepTimeLazy > 30000 ) {
      esp_deep_sleep_start(); //set the board to deep sleep mode
    }
    else {
      digitalWrite(bl, HIGH); //set the display to high
    }
  
}

void stopwatch() {
  
  //loop checks the button pin each time,
  //and will send serial if it is pressed
  bRt.loop();
  bLe.loop();

  //set the time until the stopwatch was started
  //to the current time in milliseconds
  tUntilPressed = millis();

  //print the watch face text
  printStopwatch();

  //while the right button has been pressed
  while(rtBeenPressed) {

    //set that the left button HAS NOT been pressed
    leBeenPressed = false;

    //loop checks the button pin each time,
    //and will send serial if it is pressed
    bRt.loop();
    bLe.loop();

    //set the old clock time values to the current clock time values
    clockTimeHourOld = clockTimeHour;
    clockTimeMinOld = clockTimeMin;
    clockTimeOld = clockTime;

    //set the time at which the stopwatch was started to the current time in milliseconds
    tAtPressed = millis();

    //set the stopwatch time to the time at which the stopwatch was started
    //minus the time until the stopwatch was started
    clockTime = (tAtPressed - tUntilPressed) / 1000.00;
    //same, but to int type
    clockTimeInt = (tAtPressed - tUntilPressed) / 1000;

    //increment the minutes if the seconds reach 60
    if(clockTime >= 60) {
      clockTimeMin++;
      tUntilPressed = millis();
    }
    //increment the hours if the minutes reach 60
    if(clockTimeMin >= 60) {
      clockTimeHour++;
      clockTimeMin = 0;
    }

    //print the watch face text
    printStopwatch();

    //draw the timer "squares" that appear for incrementing digits
    if(clockTimeOld != clockTimeInt ){
      //tft.fillRect(0, 150, 62, 18, TFT_GREEN);
      drawJpeg("/swSecondSquare.jpg", 0 , 150);
    }
    if(clockTimeMinOld != clockTimeMin ){
      //tft.fillRect(0, 100, 30, 18, TFT_GREEN);
      drawJpeg("/swMinuteSquare.jpg", 0 , 100);
    }
    if(clockTimeHourOld != clockTimeHour ){
      //tft.fillRect(0, 50, 50, 18, TFT_GREEN);
      drawJpeg("/swHourSquare.jpg", 0 , 50);
    }

    //if the left button is pressed
    if(leBeenPressed){
      
      //set that the right button HAS NOT been pressed
      rtBeenPressed = false;
      //set that the left button HAS NOT been pressed
      leBeenPressed = false;
      //draw the timer "squares"
      drawJpeg("/swSecondSquare.jpg", 0 , 150);
      drawJpeg("/swMinutesSquare.jpg", 0 , 100);
      drawJpeg("/swHourSquare.jpg", 0 , 50);
      
    }
    
  }
  
}

void printStopwatch() {

  tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(0, 0);
    tft.print("Stopwatch");
    tft.setCursor(0, 30);
    tft.print("Hours:");
    tft.setCursor(0, 50);
    tft.print(clockTimeHour);
    tft.setCursor(0, 80);
    tft.print("Minutes:");
    tft.setCursor(0, 100);
    tft.print(clockTimeMin);
    tft.setCursor(0, 130);
    tft.print("Seconds:");
    tft.setCursor(0, 150);
    tft.print(clockTime);
  
}

void dailyChecklist() {

  if(dailyCupsOfWaterOld != dailyCupsOfWater || dailyMealsEatenOld != dailyMealsEaten || dailyMysteryFedOld != dailyMysteryFed) {
    drawJpeg("/wallPaper.jpg", 0 , 0);
  }

  dailyCupsOfWaterOld = dailyCupsOfWater;
  dailyMealsEatenOld = dailyMealsEaten;
  dailyMysteryFedOld = dailyMysteryFed;

  //loop checks the button pin each time,
  //and will send serial if it is pressed
  bRt.loop();
  bLe.loop();

  //print the watch face text
  printDailyChecklist();
  
}

void printDailyChecklist() {

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(0, 0);
  tft.print("Daily\nChecklist");
  tft.setTextSize(2);
  
  tft.setCursor(0, 40);
  tft.print("Cups of\nwater:");
  tft.setCursor(0, 75);
  if(dailyChecklistSelection == 0) {
    tft.setTextColor(TFT_YELLOW);
  }
  tft.print(dailyCupsOfWater);
  tft.setTextColor(TFT_WHITE);
  
  tft.setCursor(0, 100);
  tft.print("Meals\neaten:");
  tft.setCursor(0, 135);
  if(dailyChecklistSelection == 1) {
    tft.setTextColor(TFT_YELLOW);
  }
  tft.print(dailyMealsEaten);
  tft.setTextColor(TFT_WHITE);
  
  tft.setCursor(0, 160);
  tft.print("Mystery\nfed:");
  tft.setCursor(0, 195);
  if(dailyChecklistSelection == 2) {
    tft.setTextColor(TFT_YELLOW);
  }
  tft.print(dailyMysteryFed);
  tft.setTextColor(TFT_WHITE);
  
}

void weatherTracker() {

  if( tempOld != temp || apparentTempOld != apparentTemp || precipIntensityOld != precipIntensity || !precipTypeOld.equals(precipType) || precipProbabilityOld != precipProbability || humidityOld != humidity || !summaryOld.equals(summary) ) {
    drawJpeg("/wallPaper.jpg", 0 , 0);
  }

  tempOld = temp;
  apparentTempOld = apparentTemp;
  precipIntensityOld = precipIntensity;
  precipTypeOld = precipType;
  precipProbabilityOld = precipProbability;
  humidityOld = humidity;
  pressureOld = pressure;
  windSpeedOld = windSpeed;
  cloudCoverOld = cloudCover;
  uvIndexOld = uvIndex;
  summaryOld = summary;

  //loop checks the button pin each time,
  //and will send serial if it is pressed
  bRt.loop();
  bLe.loop();

  //print the watch face text
  printWeatherTracker();
  
}

void printWeatherTracker() {

  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(0, 0);
  tft.print("Weather\nTracker");
  tft.setTextSize(1);
  tft.setCursor(0, 40);
  
  tft.println("Temperature:");
  tft.print(" Real: ");
  tft.println(temp);
  tft.print(" Apparent: ");
  tft.println(apparentTemp);
  tft.println();

  tft.println("Precipitation:");
  tft.print(" Intensity: ");
  tft.println(precipIntensity);
  tft.print(" Type: ");
  tft.println(precipType);
  tft.print(" Probability: ");
  tft.println(precipProbability);
  tft.println();

  tft.print("Humidity: ");
  tft.println(humidity);
  tft.println();

  tft.print("Pressure: ");
  tft.println(pressure);
  tft.println();

  tft.print("Wind Speed: ");
  tft.println(windSpeed);
  tft.println();

  tft.print("Cloud Cover: ");
  tft.println(cloudCover);
  tft.println();

  tft.print("UV Index: ");
  tft.println(uvIndex);
  tft.println();

  tft.print("Summary: ");
  tft.println(summary);
  
}

void retrieveWeatherData() {

  if(WiFi.status() == WL_CONNECTED) {

    HTTPClient http;
    http.useHTTP10(true);

    http.begin("https://api.pirateweather.net/forecast/[API_KEY]/[COORDINATES]");
    Serial.println("BEGINNING GET REQUEST");
    int timeoutStart = millis();
    int httpCode = http.GET();
    int timeoutEnd = millis();
    Serial.print("EFFECTIVE TIMEOUT: ");
    Serial.println(timeoutEnd - timeoutStart);
    
    if(httpCode > 0) {
      Serial.print("HTTP CODE: ");
      Serial.println(httpCode);
      deserializeJson(doc, http.getStream());
      Serial.println(doc.as<String>());
      
      temp = doc["currently"]["temperature"];
      apparentTemp = doc["currently"]["apparentTemperature"];
      precipIntensity = doc["currently"]["precipIntensity"];
      //precipType = doc["currently"]["precipType"];
      precipProbability = doc["currently"]["precipProbability"];
      humidity = doc["currently"]["humidity"];
      pressure = doc["currently"]["pressure"];
      windSpeed = doc["currently"]["windSpeed"];
      cloudCover = doc["currently"]["cloudCover"];
      uvIndex = doc["currently"]["uvIndex"];
      //summary = doc["currently"]["summary"];
      
    } else {
      Serial.println("ERROR ON HTTP REQUEST");
    }
    http.end();
    
  }
  
}
 
