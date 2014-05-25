/*
Modified by ms
 version 7
 
 Editor     : Manfred Schüssler
 Date       : 10.12.2013
 
 
 ms:
 - Verwendet ARDUINO MEGA 2560 Board mit LCD4884-Shiled, WLAN-Shield und Prototype-Shield
 - Folgende Werte gibt es, die in die HomeMatic geschrieben werden :
 SendItem = 0 : Oelsumme : Gesamtölverbrauch, kommt von Zählimpuls -> 1 Impuls = 1l -> INT2
 SendItem = 1 : Oelsumme6h : Ölverbrauch innerhalb von jeweils 6h
 SendItem = 2 : Brennerzustand : 1 = Brenner ein, 0 = Brenner aus -> Kommt von Digitaleingang bepl0 = 35 (Eingang PL0)
 SendItem = 3 : Betriebsminuten : Anzahl der Betriebsminuten des Brenners
 SendItem = 4 : Brennerstarts : Anzahl der Brennerstarts
 SendItem = 5 : Brennerstarts6h : Anzahl der Brennerstarts innerhalb von 6h
 - Folgende Werte gibt es, die von der HomeMatic gelesen werden :
 Datum und Uhrzeit -> kommt noch
 
 */

#include "LCD4884.h"
#include <EEPROM.h>
#include "Timer.h"
#include <SPI.h> 
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Time.h>   

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte ip[] = { 
  192, 168, 20, 225 };  
byte dnsad[] = { 
  192, 168, 20, 1 };  
byte gatewayad[] = { 
  192, 168, 20, 1 };  

// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
IPAddress server(192,168,20,220);  // numeric IP for Google (no DNS)
// char server[] = "www.google.com";    // name address for Google (using DNS)

// Initialize the Ethernet client library
// with the IP address and port of the server 
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;

unsigned int localPort = 8888;      // local port to listen for UDP packets
time_t prevDisplay = 0; // when the digital clock was displayed
const long timeZoneOffset = 3600L; // set this to the offset in seconds to your local time; -> Deutschland = +1 h = + 3600 Sekunden

IPAddress timeServer(132, 163, 4, 101); // time-a.timefreq.bldrdoc.gov NTP server
// IPAddress timeServer(132, 163, 4, 102); // time-b.timefreq.bldrdoc.gov NTP server
// IPAddress timeServer(132, 163, 4, 103); // time-c.timefreq.bldrdoc.gov NTP server

const int NTP_PACKET_SIZE= 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets 

// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

// Für Kommunikation mit HomeMatic
char Orderstring[255];
char SysVarOelsumme[] = "Oelsumme";
char SysVarOelsumme6h[] = "Oelsumme6h";
char SysVarBrennerzustand[] = "Brennerzustand";
char SysVarBetriebsminuten[] = "Betriebsminuten";
char SysVarBrennerstarts[] = "Brennerstarts";
char SysVarBrennerstarts6h[] = "Brennerstarts6h";
char SysVarTest[] = "Test";
boolean StatusOrderstringSent = false;
boolean StatusOrderstringAnswered = false;
boolean StatusOrderstringAnswerTimeout = false;
boolean SendOrderstring = false;
//Brennerstart debounce parameter
#define DEBOUNCE_BRENNER_START 20 // Eingang muss mindestens 20 Sekunden =0 (Brenner ein) sein
#define DEBOUNCE_BRENNER_STOP 60 // Eingang muss mindestens 60 Sekunden =1 (Brenner aus) sein
#define DEBOUNCE_OELZAEHLER 30 // Frühestens nach 30 Sekunden ein neuer gültiger Zählwert

// int SendItem = 0; // Welcher Wert soll an die CCU gesendet werden

//keypad debounce parameter
#define DEBOUNCE_MAX 15
#define DEBOUNCE_ON  10
#define DEBOUNCE_OFF 3 

#define NUM_KEYS 5

#define NUM_MENU_ITEM	4

// joystick number
#define LEFT_KEY 0
#define CENTER_KEY 1
#define DOWN_KEY 2
#define RIGHT_KEY 3
#define UP_KEY 4

// menu starting points

#define MENU_X	0		// 0-83
#define MENU_Y	0		// 0-5


int  adc_key_val[5] ={
  50, 200, 400, 600, 800 };

// debounce counters
byte button_count[NUM_KEYS];
// button status - pressed/released
byte button_status[NUM_KEYS];
// button on flags for user program 
byte button_flag[NUM_KEYS];


// Pin 13 has an LED connected on most Arduino boards.
// give it a name:
int ledgn1 = 22; // 1. Grüne LED
int ledye1 = 23; // 1. Gelbe LED
int ledrd1 = 24; // 1. Rote LED
int ledgn2 = 25; // 2. Grüne LED
int ledye2 = 26; // 2. Gelbe LED
int ledrd2 = 27; // 2. Rote LED
int ledrd = 28; // Letzte (Rote) LED

int bepl0 = 35; // Eingang PL0
int int2 = 21; // Eingang PD0 = INT2

// Variables will change:
int ledState = LOW;             // ledState used to set the LED
int state = LOW;
int loopstate = LOW;


int CountByte0 = 0;
int CountByte1 = 0;
int CountByte2 = 0;
int CountByte3 = 0;

// rest von der Menüstruktur des Beispiels
int current_menu_item;
long Oelsumme = 0;
long OldOelsumme = 0;
long Oelsumme6h = 0;
long OldOelsumme6h = 0;
long Betriebsminuten = 0;
long OldBetriebsminuten = 0;
long Betriebs10sek = 0;
long Brennerstarts = 0;
long Brennerstarts6h = 0;
long OldBrennerstarts6h = 0;
boolean Brennerzustand = false;
boolean FlankeBrennerzustandAus = false;
boolean FlankeBrennerzustandEin = false;
int BrennerAusBounce = 0;
int BrennerEinBounce = 0;
int OelzahlerBounce = 0;
boolean FlankeAlle6h = false;
long SysVTest = 0;
boolean SendOelsumme = false;
boolean SendOelsumme6h = false;
boolean SendBrennerzustand = false;
boolean SendBetriebsminuten = false;
boolean SendBrennerstarts = false;
boolean SendBrennerstarts6h = false;


int counter = 0;
int FirstLine = 0;
boolean BackLight = true;

Timer t;
int afterEventTimeoutAnswer;

void setup()
{
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);

  // start the Ethernet connection:
  Ethernet.begin(mac, ip, dnsad, gatewayad);
  // give the Ethernet shield a second to initialize:
  delay(1000);
  Serial.println("Setup finished");

  Udp.begin(localPort);
  Serial.println("waiting for sync");
  setSyncProvider(getNTPTime);      // set the external time provider
  setSyncInterval(3600);            // set the number of seconds between re-sync
  while(timeStatus()== timeNotSet); // wait until the time is set by the sync provider

  // setup interrupt-driven keypad arrays  
  // reset button arrays
  for(byte i=0; i<NUM_KEYS; i++){
    button_count[i]=0;
    button_status[i]=0;
    button_flag[i]=0;
  }

  // Setup timer2 -- Prescaler/256
  TCCR2A &= ~((1<<WGM21) | (1<<WGM20));
  TCCR2B &= ~(1<<WGM22);
  TCCR2B = (1<<CS22)|(1<<CS21);      

  ASSR |=(0<<AS2);

  // Use normal mode  
  TCCR2A =0;    
  //Timer2 Overflow Interrupt Enable  
  TIMSK2 |= (0<<OCIE2A);
  TCNT2=0x6;  // counting starts from 6;  
  TIMSK2 = (1<<TOIE2);    

  SREG|=1<<SREG_I;

  lcd.LCD_init();
  lcd.LCD_clear();

  lcd.backlight(ON);//Turn on the backlight
  //lcd.backlight(OFF); // Turn off the backlight

  // initialize the digital pin as an output.
  pinMode(ledgn1, OUTPUT);
  pinMode(ledye1, OUTPUT);
  pinMode(ledrd1, OUTPUT);
  pinMode(ledgn2, OUTPUT);
  pinMode(ledye2, OUTPUT);
  pinMode(ledrd2, OUTPUT);
  pinMode(ledrd, OUTPUT);
  pinMode(bepl0, INPUT);
  pinMode(int2, INPUT);
  digitalWrite(int2,HIGH);

  attachInterrupt(2, CountInterrupt, FALLING);

  int tick1SekEvent = t.every(1000, doAll1Sek, (void*)2);
  Serial.print("1 second tick started id=");
  Serial.println(tick1SekEvent);

  int tick10SekEvent = t.every(10000, doAll10Sek, (void*)2);
  Serial.print("10 second tick started id=");
  Serial.println(tick10SekEvent);

  int tick1MinEvent = t.every(60000, doAll1Min, (void*)2);
  Serial.print("1 minute tick started id=");
  Serial.println(tick1MinEvent);

  int tick1HourEvent = t.every(3600000, doAll1Hour, (void*)2); // 1h = 3.600.000 mSek
  Serial.print("1 hour tick started id=");
  Serial.println(tick1HourEvent);

  int tick6HourEvent = t.every(21600000, doAll6Hour, (void*)2); // 6h = 21.600.000 mSek
  Serial.print("6 hour tick started id=");
  Serial.println(tick6HourEvent);

  // Startwerte schreiben
  //WriteCountEEProm( 1000, 0); // Oelsumme
  //WriteCountEEProm( 0, 4); // Betriebs10sek
  //WriteCountEEProm( 0, 8); // Brennerstarts

  // Werte wieder aus EEPROM lesen
  Serial.println("EEPROM ausgelesen");
  Oelsumme = ReadCountEEProm(0);
  Serial.print("Oelsumme : ");
  Serial.println(Oelsumme, DEC);
  Betriebs10sek = ReadCountEEProm(4);
  Betriebsminuten = Betriebs10sek / 6;
  OldBetriebsminuten = Betriebsminuten;
  Serial.print("Betriebs10sek : ");
  Serial.println(Betriebs10sek, DEC);
  Brennerstarts = ReadCountEEProm(8);
  Serial.print("Brennerstarts : ");
  Serial.println(Brennerstarts, DEC);
  OldOelsumme = Oelsumme;
  OldOelsumme6h = Oelsumme;
  OldBrennerstarts6h = Brennerstarts;  
}


/* loop */

void loop() {
  CursorKeys(); 
  t.update();
  ShowSreen();
  // print out the state of the CountImpuls variable:
  digitalWrite(ledye1, state);
  digitalWrite(ledgn2, loopstate);
  digitalWrite(ledye2, !loopstate);
  loopstate = !loopstate;

  if (Oelsumme != OldOelsumme) { 
    Serial.println(Oelsumme, DEC);
    SendOelsumme = true;
    OldOelsumme = Oelsumme;
  }
  boolean val;
  val = digitalRead(bepl0);   // read the input pin
  digitalWrite(ledgn1, val);    // sets the LED to the button's value
  if (val == true) {
    Brennerzustand = 0 ;
    BrennerEinBounce = 0;
    FlankeBrennerzustandEin = false;
    if (FlankeBrennerzustandAus == false) {
      SendBrennerzustand = true;
      FlankeBrennerzustandAus = true;
      BrennerAusBounce = 0;
    }
  }
  else {
    if ((BrennerEinBounce > DEBOUNCE_BRENNER_START) & (BrennerAusBounce > DEBOUNCE_BRENNER_STOP)) {
      Brennerzustand = 1 ;
      BrennerAusBounce = 0;
      FlankeBrennerzustandAus = false;
      if (FlankeBrennerzustandEin == false) {
        Brennerstarts++;
        SendBrennerstarts = true;
        SendBrennerzustand = true;
        FlankeBrennerzustandEin = true;
      }
    }
  }
  if ((SendOelsumme == true || SendOelsumme6h == true || SendBrennerzustand == true || SendBetriebsminuten == true || SendBrennerstarts == true || SendBrennerstarts6h == true) & SendOrderstring == false) {
    SendOrderstring = true;
  }
  httpRequest();
  if( now()/1800 != prevDisplay) //update the display only if the minutes has changed
  {
    prevDisplay = now()/1800;
    digitalClockDisplay();  
  }
  if (hour() % 6 == 0) {  // Alle 6h ausführen
    if (FlankeAlle6h == false) {
      Oelsumme6h = Oelsumme - OldOelsumme6h;
      OldOelsumme6h = Oelsumme;
      SendOelsumme6h = true;
      Brennerstarts6h = Brennerstarts - OldBrennerstarts6h;
      OldBrennerstarts6h = Brennerstarts;  
      SendBrennerstarts6h = true;
      FlankeAlle6h = true;
      Serial.print("Modulo : ");
      Serial.println(hour());
    }
  }
  else {
    FlankeAlle6h = false;
  }
}

void doAfter(void* context)
{
  Serial.println("Timeout Answer");
  StatusOrderstringAnswerTimeout = true;
}

void httpRequest() {
  // if you get a connection, report back via serial:
  if (SendOrderstring == true) {
    if (StatusOrderstringSent == false) {
      if (client.connect(server, 8181)) {
        Serial.println("connected");
        // Make a HTTP request:
        if (SendOelsumme == true) {
          sprintf(Orderstring,"GET /loksoft.exe?ret=dom.GetObject(\"%s\").State(%d) HTTP/1.1",SysVarOelsumme, Oelsumme);
          SendOelsumme = false;
        }
        else if (SendOelsumme6h == true) {
          sprintf(Orderstring,"GET /loksoft.exe?ret=dom.GetObject(\"%s\").State(%d) HTTP/1.1",SysVarOelsumme6h, Oelsumme6h);
          SendOelsumme6h = false;
        }
        else if (SendBrennerzustand == true) {
          sprintf(Orderstring,"GET /loksoft.exe?ret=dom.GetObject(\"%s\").State(%d) HTTP/1.1",SysVarBrennerzustand, Brennerzustand);
          SendBrennerzustand = false;
        }
        else if (SendBetriebsminuten == true) {
          sprintf(Orderstring,"GET /loksoft.exe?ret=dom.GetObject(\"%s\").State(%d) HTTP/1.1",SysVarBetriebsminuten, Betriebsminuten);
          SendBetriebsminuten = false;
        }
        else if (SendBrennerstarts == true) {
          sprintf(Orderstring,"GET /loksoft.exe?ret=dom.GetObject(\"%s\").State(%d) HTTP/1.1",SysVarBrennerstarts, Brennerstarts);
          SendBrennerstarts = false;
        }
        else if (SendBrennerstarts6h == true) {
          sprintf(Orderstring,"GET /loksoft.exe?ret=dom.GetObject(\"%s\").State(%d) HTTP/1.1",SysVarBrennerstarts6h, Brennerstarts6h);
          SendBrennerstarts6h = false;
        }
        else {
          sprintf(Orderstring,"GET /loksoft.exe?ret=dom.GetObject(\"%s\").State(%d) HTTP/1.1",SysVarTest, ++SysVTest);
        }
        client.println(Orderstring);
        Serial.println(Orderstring);
        client.println("Host: www.homematic.de");
        client.println("Connection: close");
        client.println();
      } 
      else {
        // if you didn't get a connection to the server:
        Serial.println("connection failed");
      }
      StatusOrderstringSent = true;
      afterEventTimeoutAnswer = t.after(10000, doAfter, (void*)10);
      //Serial.print("After event started id=");
      //Serial.println(afterEventTimeoutAnswer); 

    }
    // if the server's disconnected, stop the client:
    if (StatusOrderstringAnswerTimeout == true ||  client.available()) {
      //delay(5000);
      while (client.available()) {
        char c = client.read();
        //Serial.print(c);
      }
      Serial.flush();
      StatusOrderstringAnswered = true;
    }
    if (StatusOrderstringSent == true &&  StatusOrderstringAnswered == true) {
      client.stop();
      Serial.println("disconnected");
      t.stop(afterEventTimeoutAnswer);
      StatusOrderstringSent = false;
      StatusOrderstringAnswered = false;
      StatusOrderstringAnswerTimeout = false;
      SendOrderstring = false;
    }
  }
}

void ShowSreen(void){
  int x = 0;
  int y = 0;
  if (FirstLine == 0) {
    fDatumUhrzeit(x, y);
    y = y+2;
    fOelsumme(x, y);
    y = y+2; 
    fOelsumme6h(x, y);
  }
  else if (FirstLine == 1) {
    fOelsumme(x, y);
    y = y+2; 
    fOelsumme6h(x, y);
    y = y+2;
    fBetriebsminuten(x, y);
  }
  else if (FirstLine == 2) {
    fOelsumme6h(x, y);
    y = y+2;
    fBetriebsminuten(x, y);
    y = y+2;
    fBrennerstarts(x, y);
  }
  else if (FirstLine == 3) {
    fBetriebsminuten(x, y);
    y = y+2;
    fBrennerstarts(x, y);
    y = y+2;
    fBrennerstarts6h(x, y);
  }
}

void FloatToString( float val, unsigned int precision, char* Dest){
  // prints val with number of decimal places determine by precision
  // NOTE: precision is 1 followed by the number of zeros for the desired number of decimial places
  // example: printDouble( 3.1415, 100); // prints 3.14 (two decimal places)
  // Gibt den String in der globalen char-array variablen Str zurück
  long frac;
  char Teil1[30] = "";
  char Teil2[10] = "";
  if(val >= 0)
    frac = (val - long(val)) * precision;
  else
    frac = (long(val)- val ) * precision;
  ltoa(long(val), Teil1, 10);
  ltoa(frac, Teil2, 10);
  //Serial.println("Teil1");
  //PrintChar(Teil1);
  //Serial.println("Teil2");
  //PrintChar(Teil2);
  strcat(Teil1, ".");
  strcat(Teil1, Teil2);
  strcpy(Dest, Teil1);
} 

void PrintChar(char* Str){
  int i = 0;
  do 
  {
    Serial.println(Str[i]);
    i = i+1;
  } 
  while (Str[i-1] != '\0');
}

void fDatumUhrzeit(int x, int y){
  char Str1[100] = ""; // Fehler beim Kompilieren bei Anzahl < 70 ???
  lcd.LCD_write_string(x, y, "Datum/Uhrzeit", MENU_NORMAL);  
  sprintf(Str1,"%02d.%02d.%02d/%02d:%02d",day(),month(),year()-2000,hour(),minute());
  lcd.LCD_write_string(x, y+1, Str1, MENU_NORMAL);  
}


void fOelsumme(int x, int y){
  char Str1[100] = ""; // Fehler beim Kompilieren bei Anzahl < 70 ???
  ltoa(Oelsumme, Str1,10);
  strcat(Str1, " l");
  lcd.LCD_write_string(x, y, "Oel Gesamtsumme", MENU_NORMAL);  
  lcd.LCD_write_string(x, y+1, Str1, MENU_NORMAL);  
  //Serial.println("Str1");
  //Serial.println(Str1);
  //PrintChar(Str1);
}

void fOelsumme6h(int x, int y){
  char Str1[100] = ""; // Fehler beim Kompilieren bei Anzahl < 70 ???
  //FloatToString(Oelsumme6h, 10, Str1);
  ltoa(Oelsumme6h, Str1,10);
  strcat(Str1, " l/6h");
  lcd.LCD_write_string(x, y, "Oel Summe/6h", MENU_NORMAL);  
  lcd.LCD_write_string(x, y+1, Str1, MENU_NORMAL);  
}

void fBetriebsminuten(int x, int y){
  char Str1[100] = ""; // Fehler beim Kompilieren bei Anzahl < 70 ???
  ltoa(Betriebsminuten, Str1,10);
  strcat(Str1, " min");
  lcd.LCD_write_string(x, y, "Betriebsminuten", MENU_NORMAL);  
  lcd.LCD_write_string(x, y+1, Str1, MENU_NORMAL);  
}

void fBrennerstarts(int x, int y){
  char Str1[100] = ""; // Fehler beim Kompilieren bei Anzahl < 70 ???
  ltoa(Brennerstarts, Str1,10);
  strcat(Str1, " Starts");
  lcd.LCD_write_string(x, y, "Brennerstarts", MENU_NORMAL);  
  lcd.LCD_write_string(x, y+1, Str1, MENU_NORMAL);  
}

void fBrennerstarts6h(int x, int y){
  char Str1[100] = ""; // Fehler beim Kompilieren bei Anzahl < 70 ???
  ltoa(Brennerstarts6h, Str1,10);
  strcat(Str1, " Starts/6h");
  lcd.LCD_write_string(x, y, "Brnstarts/6h", MENU_NORMAL);  
  lcd.LCD_write_string(x, y+1, Str1, MENU_NORMAL);  
}

void CountInterrupt() {
  state = !state;
  if (OelzahlerBounce > DEBOUNCE_OELZAEHLER) {
    OelzahlerBounce = 0;
    Oelsumme++;
  }
}

// Timer2 interrupt routine -
// 1/(160000000/256/(256-6)) = 4ms interval
ISR(TIMER2_OVF_vect) {  
  TCNT2  = 6;
  update_adc_key();
}

long ReadCountEEProm(int BaseAddress){
  long Count = 0;
  int Shift = BaseAddress + 3;
  //Serial.println("EEPROM Byte lesen");
  for (int i = BaseAddress; i < BaseAddress+4; i++) {
    Count = Count << 8;
    Count = Count | long(EEPROM.read(Shift));
    //Count = Count + (long(EEPROM.read(i)) << (Shift * 8));
    Shift--;
    //Serial.print(Shift);
    //Serial.print(" ");
    //Serial.print(EEPROM.read(Shift), HEX);
    //Serial.print(" ");
  }
  //Serial.print(" EEPROM gelesen : ");
  //Serial.println(Count, HEX);
  return Count;
}

void WriteCountEEProm(long Count, int BaseAddress){
  int ByteX;
  //Serial.println("EEPROM vor schreiben");
  //Serial.println(Count, HEX);
  //Serial.println("EEPROM Byte schreiben");
  for (int i = BaseAddress; i < BaseAddress+4; i++){
    ByteX = int(Count & 0x000000FF);
    //Serial.print(i);
    //Serial.print(" ");
    //Serial.print(ByteX, HEX);
    //Serial.print(" ");
    EEPROM.write(i, ByteX);
    Count = Count >> 8;
  }
  //Serial.print(" EEPROM geschrieben :");
  //Serial.println(Count, HEX);
  //Serial.println("EEPROM wieder auslesen");
  //Serial.println(ReadCountEEProm(0), HEX);
}

void EEPromWrite(void){
  Serial.println("EEPROM geschrieben");
  WriteCountEEProm(Oelsumme , 0);
  Serial.print("Oelsumme  : ");
  Serial.println(Oelsumme , DEC);
  Serial.println(ReadCountEEProm(0), DEC);
  WriteCountEEProm(Betriebs10sek, 4);
  Serial.print("Betriebs10sek : ");
  Serial.println(Betriebs10sek, DEC);
  Serial.println(ReadCountEEProm(4), DEC);
  WriteCountEEProm(Brennerstarts, 8);
  Serial.print("Brennerstarts : ");
  Serial.println(Brennerstarts, DEC);
  Serial.println(ReadCountEEProm(8), DEC);
}

void doAll1Sek(void* context) {
  //int time = (int)context;
  //Serial.print(time);
  //Serial.print(" 1 second tick: millis()=");
  //Serial.println(millis());
  BrennerAusBounce++;
  BrennerEinBounce++;
  OelzahlerBounce++;
  // set the LED with the ledState of the variable:
  if (ledState == LOW)
    ledState = HIGH;
  else
    ledState = LOW;
  digitalWrite(ledrd, ledState);
}

void doAll10Sek(void* context) {
  int time = (int)context;
  //Serial.print(time);
  //Serial.print(" 10 second tick: millis()=");
  //Serial.println(millis());
  if (Brennerzustand == 1) {
    Betriebs10sek++ ;
  }
  Betriebsminuten = Betriebs10sek / 6 ;
  if (Betriebsminuten != OldBetriebsminuten) { 
    SendBetriebsminuten = true;
    OldBetriebsminuten = Betriebsminuten;
  }
}

void doAll1Min(void* context) {
  int time = (int)context;
  //Serial.print(time);
  //Serial.print(" 1 minute tick: millis()=");
  //Serial.println(millis());
}

void doAll1Hour(void* context) {
  int time = (int)context;
  Serial.print(time);
  Serial.print(" 1 hour tick: millis()=");
  Serial.println(millis());
  EEPromWrite();
}

void doAll6Hour(void* context) {
  int time = (int)context;
  Serial.print(time);
  Serial.print(" 6 hour tick: millis()=");
  Serial.println(millis());
}

void CursorKeys(void) {
  byte i;
  char buf[14];
  for(i=0; i<NUM_KEYS; i++){
    if(button_flag[i] !=0){

      button_flag[i]=0;  // reset button flag
      switch(i){

      case UP_KEY:
        current_menu_item -= 1;
        if(current_menu_item < 0)  current_menu_item = NUM_MENU_ITEM -1;
        FirstLine += 1;
        if (FirstLine > 3)   FirstLine = 3; 
        break;
      case DOWN_KEY:
        current_menu_item += 1;
        if(current_menu_item > (NUM_MENU_ITEM-1))  current_menu_item = 0;
        FirstLine -= 1;
        if (FirstLine < 0)   FirstLine = 0; 
        break;
      case LEFT_KEY:
        current_menu_item = 0;
        break;   
      case CENTER_KEY:
        current_menu_item = 0;
        if (BackLight) {
          BackLight = false;
          lcd.backlight(OFF);
        }
        else {
          BackLight = true;    
          lcd.backlight(ON);
        }
        break;	
      }
      lcd.LCD_clear();
    }
  }
}
// waiting for center key press
void waitfor_OKkey(){
  byte i;
  byte key = 0xFF;
  while (key!= CENTER_KEY){
    for(i=0; i<NUM_KEYS; i++){
      if(button_flag[i] !=0){
        button_flag[i]=0;  // reset button flag
        if(i== CENTER_KEY) key=CENTER_KEY;
      }
    }
  }

}


// The followinging are interrupt-driven keypad reading functions
// which includes DEBOUNCE ON/OFF mechanism, and continuous pressing detection


// Convert ADC value to key number
char get_key(unsigned int input) {
  char k;

  for (k = 0; k < NUM_KEYS; k++)
  {
    if (input < adc_key_val[k])
    {

      return k;
    }
  }

  if (k >= NUM_KEYS)
    k = -1;     // No valid key pressed

  return k;
}

void update_adc_key() {
  int adc_key_in;
  char key_in;
  byte i;

  adc_key_in = analogRead(0);
  key_in = get_key(adc_key_in);
  for(i=0; i<NUM_KEYS; i++)
  {
    if(key_in==i)  //one key is pressed 
    { 
      if(button_count[i]<DEBOUNCE_MAX)
      {
        button_count[i]++;
        if(button_count[i]>DEBOUNCE_ON)
        {
          if(button_status[i] == 0)
          {
            button_flag[i] = 1;
            button_status[i] = 1; //button debounced to 'pressed' status
          }

        }
      }

    }
    else // no button pressed
    {
      if (button_count[i] >0)
      {  
        button_flag[i] = 0;	
        button_count[i]--;
        if(button_count[i]<DEBOUNCE_OFF){
          button_status[i]=0;   //button debounced to 'released' status
        }
      }
    }

  }
}

unsigned long getNTPTime() {
  sendNTPpacket(timeServer); // send an NTP packet to a time server
  // wait to see if a reply is available
  delay(1000);  
  if ( Udp.parsePacket() ) {  
    // We've received a packet, read the data from it
    Udp.read(packetBuffer,NTP_PACKET_SIZE);  // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);  
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;  
    // now convert NTP time into everyday time:
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;     
    // subtract seventy years and add the timezone:
    unsigned long epoch = secsSince1900 - seventyYears  + timeZoneOffset;  
    return epoch;
  }
  return 0; // return 0 if unable to get the time
}


// send an NTP request to the time server at the given address 
unsigned long sendNTPpacket(IPAddress& address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE); 
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49; 
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp: 		   
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer,NTP_PACKET_SIZE);
  Udp.endPacket(); 
}

void digitalClockDisplay(){
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year()); 
  Serial.println(); 
}

void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}















