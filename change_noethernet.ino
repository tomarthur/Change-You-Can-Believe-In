/*
 Tom Arthur
 PComp Fall 2012
 
 Change You Can Believe In 

 Coin collector that displays the worth of your money in tangiable things scanned by an NFC reader.
 	
 Combined_v6
 
 Code Attribution
 
 Ethernet Code Framework
 modified 9 Apr 2012
 by Tom Igoe
 
 SD Code Framework
 created   Nov 2010
 by David A. Mellis
 modified 9 Apr 2012
 by Tom Igoe
 
 readMiFare for Adafruit NFC Shield Framework
 @author   Adafruit Industries
 	 @license  BSD (see license.txt)
 
 OLED & LCD Displays Framework
 Limor Fried/Ladyada for Adafruit Industries
 
 
 */

#include <SPI.h>
#include <Wire.h>
#include <SD.h>

// #include <Ethernet.h>
// #include <EthernetUdp.h>
#include <Adafruit_NFCShield_I2C.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_HX8340B.h>

// Ethernet (hopefully DHCP)
// byte mac[] = { 
//   0x90, 0xA2, 0xDA, 0x00, 0x81, 0x36};
// EthernetClient client;
// byte server[] = {128,122,157,182};
// IPAddress ip(10,0,1,27);

// // Ethernet realtime
// unsigned int localPort = 8888;      // local port to listen for UDP packets
// IPAddress timeServer(64, 90, 182, 55); // time.nist.gov NTP server
// const int NTP_PACKET_SIZE= 48; // NTP time stamp is in the first 48 bytes of the message
// byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets 
// // A UDP instance to let us send and receive packets over UDP
// EthernetUDP Udp;
// unsigned long epoch;

//LCD
#define TFT_MOSI  51     // SDI
#define TFT_CLK   52     // SCL
#define TFT_CS    30     // CS
#define TFT_RESET 49     // RESET
#define SD_CS 31		     // SD Card Chip Select

Adafruit_HX8340B lcdDSP(TFT_RESET, TFT_CS);

// Color definitions
#define lcdBLACK           0x0000
#define lcdBLUE            0x001F
#define lcdRED             0xF800
#define lcdGREEN           0x07E0
#define lcdCYAN            0x07FF
#define lcdMAGENTA         0xF81F
#define lcdYELLOW          0xFFE0  
#define lcdWHITE           0xFFFF

File bmpFile;
int bmpWidth, bmpHeight;
uint8_t bmpDepth, bmpImageoffset;

//OLED 
#define OLED_DC 38
#define OLED_CS 34
#define OLED_CLK 52
#define OLED_MOSI 51
#define OLED_RESET 36
#define LOGO16_GLCD_HEIGHT 32 
#define LOGO16_GLCD_WIDTH  16 

Adafruit_SSD1306 oLED(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);


//NFC
#define IRQ   (22)
#define RESET (3)  // Not connected by default on the NFC Shield

Adafruit_NFCShield_I2C nfc(IRQ, RESET);

File myFile;
File coinLog;
File totalCount;
File nfcScan;


// Coin Counting 
const int button = 8;
const int proxPin = A12;
const int dimePin = A8;
const int pennyPin = A9;
const int nickelPin = A10;
const int quarterPin = A11;
const int motorPin = 7;
// int lcdBack = xx;

int dimeReg, pennyReg, nickelReg, quarterReg;
int dimeCount, pennyCount, nickelCount, quarterCount, totalforSD;
// int lastDimeCount, lastPennyCount, lastNickelCount, lastQuarterCount;
int lastVal;

float saved, motorWait, total, lastTotal;
long millisDime, millisNickel, millisPenny, millisQuarter, lastPost, countStart, countFinished, tagStart, lastTime;
boolean addedDime, addedQuarter, addedNickel, addedPenny, counting, updateReady, httpReady, refresh, tagReport, sdDone, webDone;


void setup(){
  Serial.begin(9600);

  // start the Ethernet connection:
  // Serial.print("Initializing Ethernet...");
  // Ethernet.begin(mac, ip);

  // Udp.begin(localPort);

  // setup pins for reading coins

  pinMode(button, INPUT);                // currently setup for a button
  digitalWrite(button, HIGH);            // internal pull-up
  pinMode(motorPin, OUTPUT);              // relay for motor control
  counting = false;                       // not in counting mode to start
  dimeReg = analogRead(dimePin);          // get a baseline reading for all sensors
  pennyReg = analogRead(pennyReg);
  quarterReg = analogRead(quarterReg);
  nickelReg =  analogRead(nickelReg);

  SPI.begin();

  // Display startups
  Serial.println("Initializing OLED & LCD...");
  oLED.begin(SSD1306_SWITCHCAPVCC);
  lcdDSP.begin();

  // OLED 
  oLED.clearDisplay();   // clears the screen and buffer
  oLED.setTextSize(2);
  oLED.setTextColor(WHITE);
  oLED.setCursor(0,0);
  oLED.println("Starting..");
  oLED.display();
  delay(500);

  // LCD 
  lcdDSP.begin();
  lcdDSP.fillScreen(lcdBLACK);
  delay(500);


  // Ethernet SD card startup -- turned off because it was casuing conflicts with SD card on display
  Serial.println("Initializing Ethernet SD card...");
  oLED.clearDisplay();   // clears the screen and buffer
  oLED.setTextSize(2);
  oLED.setTextColor(WHITE);
  oLED.setCursor(0,0);
  oLED.println("Starting  SD Card...");
  oLED.display();


  if (!SD.begin(4)) {
    Serial.println("initialization failed for etherSD!");
    oLED.clearDisplay();   // clears the screen and buffer
    oLED.setTextSize(2);
    oLED.setTextColor(WHITE);
    oLED.setCursor(0,0);
    oLED.println("SD Card Fail");
    oLED.display();
    return;
  }
  Serial.println("initialization done for EtherSD.");


  // NFC shield startup
  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.println("Didn't find PN53x board");
    while (1); // halt
  }

  nfc.SAMConfig();   // configure board to read RFID tags

  Serial.println("Ready for an ISO14443A Card ...");


  // // LCD SD Card Startup
  // Serial.print("Initializing LCD SD card..");
  // pinMode(SD_CS, OUTPUT); // for SD card on LCD
  // digitalWrite(SD_CS, HIGH);// for SD card on LCD

  // if (!SD.begin(SD_CS)) {
  //   Serial.println("failed!");
  //   return;
  // }
  // Serial.println("LCD SD OK!");

  bmpDraw("str.bmp", 0, 0);

  oLED.clearDisplay();
  oLED.display(); 

  // get last value from file
  readChangeSD();
  total = lastTotal;

  Serial.println("all systems go!");

}

void loop(){
  changeCount();  // check if we should be counting coins

  if (counting == false && updateReady == false) {  // check to see if a NFC tag is around
    tagScan();
  }

  if (updateReady == true){ // update totals when available
    writeChangeSD();
  }

  if (refresh == true && millis() > countFinished + 4000){  // update OLED display if total has changed
    refresh = false;
    newTotalOLED(total);
    // delay(100);
    // webTotal();
    resetCounts();
  }

  if (tagReport == true && millis() > tagStart + 5000){
    tagReport = false;
    screenReset();
  }

  // if (tagReport == false && counting == false && updateReady == false && millis() > lastTime + 50000){
  //   getTime();
  // }

}

void changeCount() { // where we count new change

  if (analogRead(proxPin) > 525 && counting == false) {  // start collecting coins when button/prox is activated
    counting = true;                                       // we are counting!
    countStart = millis();                                 // know when count started 
    digitalWrite(motorPin, HIGH);                          // start motor

    oLED.clearDisplay();   // clears the screen and buffer
    oLED.display();
    oLED.setCursor(0, 0);
    oLED.setTextSize(2);
    oLED.setTextColor(BLACK, WHITE); // 'inverted' text
    oLED.println("Counting  Coins...     ");
    oLED.display();

    Serial.println("count mode");

    changeCount();
  }

  if (millis() > countStart + 5000 && counting == true) {
    counting = false;                // end count mode
    digitalWrite(motorPin, LOW);    // turn motor off

    Serial.println("end count mode");
    Serial.print("Penny: ");
    Serial.print(pennyCount);
    Serial.print(" Nickel: ");
    Serial.print(nickelCount);
    Serial.print(" Dime: ");
    Serial.print(dimeCount);
    Serial.print(" Quarter: ");
    Serial.println(quarterCount);
    updateReady = true;
  }

  if (counting == true) {

    int dimeVal = analogRead(dimePin);
    int pennyVal = analogRead(pennyPin);
    int nickelVal = analogRead(nickelPin);
    int quarterVal = analogRead(quarterPin);

    if (dimeVal < 300) {
      millisDime = millis();        // using a millis timer to prevent double counting coins
      if (addedDime == false){      // boolean protects from adding too many dimes
        dimeCount++;
        addedDime = true;
      }                       
    }

    if (millis() > (millisDime + 40)){  // after timer expires, it's ok to have a new dime
      addedDime = false;
    }

    if (pennyVal < 300) {
      millisPenny = millis();
      if (addedPenny == false){
        pennyCount++;
        addedPenny = true;
      }
    }

    if (millis() > (millisPenny + 50)){
      addedPenny = false;
    }

    if (nickelVal < 300) {
      millisNickel = millis();
      if (addedNickel == false){
        nickelCount++;
        addedNickel = true;
      }
    }

    if (millis() > (millisNickel + 40)){
      addedNickel = false;
    }

    if (quarterVal < 300) {
      millisQuarter = millis();
      if (addedQuarter == false){
        quarterCount++;
        addedQuarter = true;
      }
    }

    if (millis() > (millisQuarter + 40)){
      addedQuarter = false;
    }
  }
}

void readChangeSD() { // read the saved state of the coin collector
  // re-open the file for reading:
  totalCount = SD.open("TCOUNT.TXT");
  const int LEN = 18;
  char array[LEN+1]; // +1 allows space for the null terminator
  int counter = 0;
  float value;
  float valReal;

  if(totalCount.available()){
    while (totalCount.available())
    {

      char c = totalCount.read();

      if(c == ',')
      {
        if(counter > 0)
        {
          Serial.print("Buffer: [");
          Serial.print(array);
          Serial.println("]");
          value = atof(array);
          // Serial.print("Value: ");
          // Serial.println(value); 
        }
        counter = 0;
      }
      else
      {
        if(counter < LEN)
        {
          array[counter++] = c; // append the character
          array[counter] = 0; // null-terminate the array 
          // Serial.println(array);
        }
      }

    } 
    totalCount.close();

    Serial.println(value);
    valReal = value;
    lastVal = value;
    lastTotal = valReal / 100;
    Serial.println(lastTotal); 
    newTotalOLED(lastTotal);
  }
  else {
    // if the file didn't open, print an error:
    Serial.println("error opening tcount.txt");
    lastTotal = 0;
    lastVal = 0;
    Serial.println(lastTotal); 
    newTotalOLED(lastTotal);

  }
}

void writeChangeSD() {
  if (sdDone == false){
    totalforSD = lastVal + ((pennyCount*1)+(nickelCount*5)+(dimeCount*10)+(quarterCount*25));
    lastVal = totalforSD;
    float thisTotal = ((pennyCount*.01)+(nickelCount*.05)+(dimeCount*.10)+(quarterCount*.25));
    total = total + ((pennyCount*.01)+(nickelCount*.05)+(dimeCount*.10)+(quarterCount*.25));
    Serial.println(totalforSD);

    countFinished = millis(); // clearing everything out
    oLED.clearDisplay();
    oLED.display();
    oLED.setTextSize(1);
    oLED.setTextColor(BLACK, WHITE); // 'inverted' text
    oLED.setCursor(0,0);
    oLED.println("Nice. You collected:");
    oLED.setTextSize(2);
    oLED.println(thisTotal);
    oLED.setTextSize(1);
    // oLED.println("Sending to web...");
    oLED.display();

    coinLog = SD.open("clog.txt", FILE_WRITE);      //write to individual coin count logs
    // if the file opened okay, write to it:
    if (coinLog) {
      Serial.print("Writing to count.txt...");
      coinLog.print(lastTotal);
      coinLog.print(",");
      coinLog.print(thisTotal);   // how much we collected this time
      coinLog.print(",");
      coinLog.print(pennyCount);  // how many pennies
      coinLog.print(",");
      coinLog.print(nickelCount); // how many nickels
      coinLog.print(",");
      coinLog.print(dimeCount);   // how many dimes
      coinLog.print(",");
      coinLog.print(quarterCount); // how many quarters
      coinLog.print(",");
      coinLog.println(total);      // all together total
      coinLog.close();             // close the file:
    }
    else {
      // if the file didn't open, print an error:
      Serial.println("error opening clog.txt");
    }

    // now update the total count file, used for saving the state of the machine
    // delete the old file:
    Serial.println("Removing tcount.txt...");
    SD.remove("tcount.txt");

    if (SD.exists("tcount.txt")){ 
      Serial.println("tcount.txt exists.");
    }
    else {
      Serial.println("tcount.txt doesn't exist.");  
    }


    totalCount = SD.open("tcount.txt", FILE_WRITE);    //write to total log
    // if the file opened okay, write to it:
    if (totalCount) {
      Serial.println("Writing to tcount.txt...");
      totalCount.print(totalforSD);
      Serial.println(totalforSD);
      totalCount.println(",");
      totalCount.close(); // close the file:
    }
    else {
      // if the file didn't open, print an error:
      Serial.println("error opening tcount.txt");
    }

    lastTotal = total;
    refresh = true;           // indication that we should update the OLED display
    sdDone = true;

  }

} 



// void webTotal() {    // maybe need a timer in here to prevent things from getting stuck, or to allow other commands to override. 
//  if (webDone == false){
//     int lasttotalforweb = lastTotal * 100; 
//     int totalforweb = total * 100;
//     String urlbuild = "GET http://itp.nyu.edu/~tca241/sinatra/change/updatetotal?"; // Where we drop stuff off for Sinatra
//     urlbuild+= "&lasttotal=";
//     urlbuild+= lasttotalforweb;
//     urlbuild+= "&penny=";
//     urlbuild+= pennyCount;
//     urlbuild+= "&nickel=";
//     urlbuild+= nickelCount;
//     urlbuild+= "&dime=";
//     urlbuild+= dimeCount;
//     urlbuild+= "&quarter=";
//     urlbuild+= quarterCount;
//     urlbuild+= "&total=";
//     urlbuild+= totalforweb;
//     // Serial.println(urlbuild);

//     Serial.println("Trying to connect to web");

//     if (client.connect(server, 80)) {
//       Serial.println("connected...");
//       // Make a HTTP request:
//       client.print(urlbuild);
//       client.println(" HTTP/1.1");
//       client.println("Host: itp.nyu.edu");
//       client.println();
//       webDone = true;      // we've calculated and saved the funds, update complete.
//     } 
//     else {
//       // kf you didn't get a connection to the server:
//       Serial.println("connection failed");
//     }
//           // if there are incoming bytes available 
//   // from the server, read them and print them:
//   if (client.available()) {
//     char c = client.read();
//     Serial.print(c);
//   }

//   // if the server's disconnected, stop the client:
//   if (!client.connected()) {
//     Serial.println();
//     Serial.println("disconnecting.");
//     client.stop();
//   }
  
//  Serial.println("done with the web");
  
//  }
// }

void resetCounts(){
  pennyCount = 0;
  nickelCount = 0;
  dimeCount = 0;
  quarterCount = 0;
  updateReady = false;
  // webDone = false;
  sdDone = false;
}

void tagScan(){ // ended up just checking the tag serial, hopefully can actually read NFC data in the future
  uint8_t success;
  uint8_t uid[] = { 
    0, 0, 0, 0, 0, 0, 0   }; // Buffer to store the returned UID
  uint8_t uidLength; // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

  // // wait for RFID card to show up!
  // Serial.println("Waiting for an ISO14443A Card ...");


  // Wait for an ISO14443A type cards (Mifare, etc.). When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

  uint32_t cardidentifier = 0;

  if (success) {
    // Found a card!
    tagReport = true;

    Serial.print("Card detected #");
    // turn the four byte UID of a mifare classic into a single variable #
    cardidentifier = uid[3];
    cardidentifier <<= 8; 
    cardidentifier |= uid[2];
    cardidentifier <<= 8; 
    cardidentifier |= uid[1];
    cardidentifier <<= 8; 
    cardidentifier |= uid[0];
    Serial.println(cardidentifier);

    // repeat this for loop as many times as you have RFID cards
    if (cardidentifier == 3290310802) { // this is the card's unique identifier
      // oledStatus("got it!   checking");
      int num = int(total / 3.25);
      float more = 3.25 - total;
      if (num > 1){
        tagOLED("Starbucks Lates", num);
        bmpDraw("sbx.bmp", 0, 0);
        tagLCD("Starbucks Lates", num, more);
      }
      else{
        tagOLED("a Starbucks Late", num);
        bmpDraw("sbx.bmp", 0, 0);
        tagLCD("a Starbucks   Late", num, more);
      }
      tagStart = millis();
      writeNFCscan(cardidentifier, num, more);
    }
    if (cardidentifier == 3290329938) { // this is the card's unique identifier
      // oledStatus("got it.   checking...");
      int num = int(total / 20);
      float more = 20 - total;
      if (num > 1){
        tagOLED("Pairs of Mouse Ears", num);
        bmpDraw("mou.bmp", 0, 0);
        tagLCD("Pairs of Mouse Ears", num, more);
      }
      else{
        tagOLED("a     Pair of Mouse Ears", num);
        bmpDraw("mou.bmp", 0, 0);
        tagLCD("a Pair of     Mouse Ears", num, more);
      }
      tagStart = millis();
      writeNFCscan(cardidentifier, num, more);
    }
    if (cardidentifier == 3290290146) { // this is the card's unique identifier
      // oledStatus("got it.   checking...");
      int num = int(total / 20);
      float more = 7 - total;

      if (num > 1){
        tagOLED("Bottles of Beer", num);
        bmpDraw("bee.bmp", 0, 0);
        tagLCD("Bottles of Beer", num, more);
      }
      else{
        tagOLED("a Bottle of Beer", num);
        bmpDraw("bee.bmp", 0, 0);
        tagLCD("a Bottle of Beer", num, more);
      }
      // bmpDraw("str.bmp", 0, 0);
      tagStart = millis();
      writeNFCscan(cardidentifier, num, more);
    }
        if (cardidentifier == 3290301394) { // this is the card's unique identifier
      // oledStatus("got it.   checking...");
      int num = int(total / 3);
      float more = 3 - total;

      if (num > 1){
        tagOLEDgive("rescue dogs from the pound", num);
        bmpDraw("dog.bmp", 0, 0);
        tagLCDgive("rescue dogs from the pound", num, more);
      }
      else{
        tagOLEDgive("rescue a dog from the pound", num);
        bmpDraw("dog.bmp", 0, 0);
        tagLCDgive("rescue a dog from the pound", num, more);
      }
      // bmpDraw("str.bmp", 0, 0);
      tagStart = millis();
      writeNFCscan(cardidentifier, num, more);
    }
        if (cardidentifier == 3290281026) { // this is the card's unique identifier
        oledStatus("got it. checking...");
      int num = int(total / 1.00);
      float more = 1.00 - total;

      if (num > 1){
        tagOLEDgive("meals be provided from a food bank", num);
        bmpDraw("foo.bmp", 0, 0);
        tagLCDgive("meals be provided from a food bank", num, more);
      }
      else{
        tagOLEDgive("provide a meal from a food bank", num);
        bmpDraw("foo.bmp", 0, 0);
        tagLCDgive("provide a meal from a food bank", num, more);
      }
      // bmpDraw("str.bmp", 0, 0);
      tagStart = millis();
      writeNFCscan(cardidentifier, num, more);
    }
        if (cardidentifier == 3290297874) { // this is the card's unique identifier
        // oledStatus("got it. checking...");
      int num = int(total / 00.75);
      float more = .75 - total;

      if (num > 1){
        tagOLEDgive("people who are homeless get food & shelter for a night", num);
        bmpDraw("hom.bmp", 0, 0);
        tagLCDgive("people who are homeless get food & shelter for a night", num, more);
      }
      else{
        tagOLEDgive("person who is homeless get food & shelter for a night", num);
        bmpDraw("hom.bmp", 0, 0);
        tagLCDgive("person who is homeless get food & shelter for a night", num, more);
      }
      // bmpDraw("str.bmp", 0, 0);
      tagStart = millis();
      writeNFCscan(cardidentifier, num, more);
    }
  }
}



// write NFC scan to file & send to web
void writeNFCscan(int cardidentifier, int num, float more) {
  //  String forSinatra = epoch; //+ "," + ident + "," + num + "," + more;

  nfcScan = SD.open("nfc.txt", FILE_WRITE);      //write to individual coin count logs
  // if the file opened okay, write to it:
  if (nfcScan) {
    Serial.print("Writing to nfc.txt...");
    nfcScan.print(cardidentifier);      
    nfcScan.print(",");
    nfcScan.print(num);
    nfcScan.print(",");
    nfcScan.println(more);
    nfcScan.close();             // close the file:
  }
  else {
    // if the file didn't open, print an error:
    Serial.println("error opening nfc.txt");
  }

} 

// post to LCD when a tag is read
void tagLCDgive (String text, int num, float more) { // takes the name of the item & number you can buy or none
  lcdDSP.setTextSize(2);
  lcdDSP.setCursor(0, 0);
  lcdDSP.setTextColor(lcdWHITE, lcdBLACK);
  if (num > 0){
    lcdDSP.println("You've saved");
    lcdDSP.print("enough to help");
    lcdDSP.setTextSize(3);
    lcdDSP.println(num);
    lcdDSP.setTextSize(2);
    lcdDSP.println(text);
  }
  else{
    lcdDSP.println("You need to");
    lcdDSP.print("save");
    lcdDSP.setTextSize(3);
    lcdDSP.print("$");
    lcdDSP.println(more);
    lcdDSP.setTextSize(2);
    lcdDSP.print("to help ");
    lcdDSP.println(text);
  }

}

// post to OLED when a tag is read
void tagOLEDgive (String item, int num) {    // display tag information
  oLED.clearDisplay();
  oLED.display();
  if (num > 0){
    oLED.setTextSize(1);
    oLED.setTextColor(WHITE);
    oLED.setCursor(0,0);
    oLED.println("You've saved enough");
    oLED.print("to help ");
    oLED.print(num);
    oLED.print(" ");
    oLED.print(item);
  }
  else {
    oLED.setTextSize(1);
    oLED.setTextColor(WHITE);
    oLED.setCursor(0,0);
    oLED.println("You haven't saved");
    oLED.println("enough yet to help");
    oLED.print(item);
  }
  oLED.display();
}

// post to LCD when a tag is read
void tagLCD (String text, int num, float more) { // takes the name of the item & number you can buy or none
  lcdDSP.setTextSize(2);
  lcdDSP.setCursor(0, 0);
  lcdDSP.setTextColor(lcdWHITE, lcdBLACK);
  if (num > 0){
    lcdDSP.println("You've saved");
    lcdDSP.println("enough to buy");
    lcdDSP.setTextSize(3);
    lcdDSP.println(num);
    lcdDSP.setTextSize(2);
    lcdDSP.println(text);
  }
  else{
    lcdDSP.println("You need to");
    lcdDSP.println("save");
    lcdDSP.setTextSize(3);
    lcdDSP.print("$");
    lcdDSP.println(more);
    lcdDSP.setTextSize(2);
    lcdDSP.println("more to buy");
    lcdDSP.println(text);
  }

}

// post to OLED when a tag is read
void tagOLED (String item, int num) {    // display tag information
  oLED.clearDisplay();
  oLED.display();
  if (num > 0){
    oLED.setTextSize(1);
    oLED.setTextColor(WHITE);
    oLED.setCursor(0,0);
    oLED.print("You've saved enough to buy ");
    oLED.setTextSize(1);
    oLED.println(num);
    oLED.setTextSize(1);
    oLED.print(" ");
    oLED.print(item);
  }
  else {
    oLED.setTextSize(1);
    oLED.setTextColor(WHITE);
    oLED.setCursor(0,0);
    oLED.print("You haven't saved enough yet to buy ");
    oLED.print(item);
  }
  oLED.display();
}

// update total on OLED
void newTotalOLED(float _total ) {    // update total on OLED display
  oLED.clearDisplay();
  oLED.display();
  oLED.setTextSize(1);
  oLED.setTextColor(WHITE, BLACK);
  oLED.setCursor(0,0);
  oLED.println("You've saved");
  oLED.setTextSize(3);
  oLED.print("$");
  oLED.println(_total);
  oLED.display();
}

// clear the displays
void screenReset() {      // clear displys
  oLED.clearDisplay();
  oLED.display();
  newTotalOLED(total);
  delay(100);
  lcdDSP.fillScreen(lcdBLACK);
  bmpDraw("str.bmp", 0, 0);
}

void oledStatus(String status){
  oLED.clearDisplay();   // clears the screen and buffer
  oLED.setTextSize(2);
  oLED.setTextColor(WHITE);
  oLED.setCursor(0,0);
  oLED.println(status);
  oLED.display();
}

// This function opens a Windows Bitmap (BMP) file and
// displays it at the given coordinates.  It's sped up
// by reading many pixels worth of data at a time
// (rather than pixel by pixel).  Increasing the buffer
// size takes more of the Arduino's precious RAM but
// makes loading a little faster.  20 pixels seems a
// good balance.

#define BUFFPIXEL 50

void bmpDraw(char *filename, uint8_t x, uint8_t y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();

  if((x >= lcdDSP.width()) || (y >= lcdDSP.height())) return;

  Serial.println();
  Serial.print("Loading image '");
  Serial.print(filename);
  Serial.println('\'');

  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.print("File not found");
    return;
  }

  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.print("File size: "); 
    Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print("Image Offset: "); 
    Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print("Header size: "); 
    Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print("Bit Depth: "); 
      Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print("Image size: ");
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if((x+w-1) >= lcdDSP.width())  w = lcdDSP.width()  - x;
        if((y+h-1) >= lcdDSP.height()) h = lcdDSP.height() - y;

        lcdDSP.setWindow(x, y, x+w-1, y+h-1);
        for (row=0; row<h; row++) { // For each scanline...

          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
          pos = bmpImageoffset + row * rowSize;
          if(bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          // optimize by setting pins now
          for (col=0; col<w; col++) { // For each pixel...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format, push to display
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];

            //display.drawPixel(x+col, y+row, display.Color565(r,g,b));
            // optimized!
            lcdDSP.pushColor(lcdDSP.Color565(r,g,b));
          } // end pixel
        } // end scanline
        Serial.print("Loaded in ");
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFile.close();
  if(!goodBmp) Serial.println("BMP format not recognized.");
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

// void getTime(){

//   Serial.println("getting time");
//   sendNTPpacket(timeServer); // send an NTP packet to a time server

//     // wait to see if a reply is available
//   delay(1000);  
//   if ( Udp.parsePacket() ) {  
//     // We've received a packet, read the data from it
//     Udp.read(packetBuffer,NTP_PACKET_SIZE);  // read the packet into the buffer

//     // //the timestamp starts at byte 40 of the received packet and is four bytes,
//     // // or two words, long. First, esxtract the two words:

//     unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
//     unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);  
//     // combine the four bytes (two words) into a long integer
//     // this is NTP time (seconds since Jan 1 1900):
//     unsigned long secsSince1900 = highWord << 16 | lowWord;  
//     // Serial.print("Seconds since Jan 1 1900 = " );
//     // Serial.println(secsSince1900);               

//     // now convert NTP time into everyday time:
//     Serial.print("Unix time = ");
//     // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
//     const unsigned long seventyYears = 2208988800UL;     
//     // subtract seventy years:
//     epoch = secsSince1900 - seventyYears;  
//     // print Unix time:
//     Serial.println(epoch);                               
//     lastTime = millis();

//     // // print the hour, minute and second:
//     // Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
//     // Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
//     // Serial.print(':');  
//     // if ( ((epoch % 3600) / 60) < 10 ) {
//     //   // In the first 10 minutes of each hour, we'll want a leading '0'
//     //   Serial.print('0');
//     // }
//     // Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
//     // Serial.print(':'); 
//     // if ( (epoch % 60) < 10 ) {
//     //   // In the first 10 seconds of each minute, we'll want a leading '0'
//     //   Serial.print('0');
//     // }
//     // Serial.println(epoch %60); // print the second
//   }
//   // wait ten seconds before asking for the time again
//   // delay(10000); 
// }

// // send an NTP request to the time server at the given address 
// unsigned long sendNTPpacket(IPAddress& address)
// {
//   // set all bytes in the buffer to 0
//   memset(packetBuffer, 0, NTP_PACKET_SIZE); 
//   // Initialize values needed to form NTP request
//   // (see URL above for details on the packets)
//   packetBuffer[0] = 0b11100011;   // LI, Version, Mode
//   packetBuffer[1] = 0;     // Stratum, or type of clock
//   packetBuffer[2] = 6;     // Polling Interval
//   packetBuffer[3] = 0xEC;  // Peer Clock Precision
//   // 8 bytes of zero for Root Delay & Root Dispersion
//   packetBuffer[12]  = 49; 
//   packetBuffer[13]  = 0x4E;
//   packetBuffer[14]  = 49;
//   packetBuffer[15]  = 52;

//   // all NTP fields have been given values, now
//   // you can send a packet requesting a timestamp:       
//   Udp.beginPacket(address, 123); //NTP requests are to port 123
//   Udp.write(packetBuffer,NTP_PACKET_SIZE);
//   Udp.endPacket(); 
// }



