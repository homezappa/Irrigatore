/* *********************************************************************************
* ZAP Irrigatore 1.0
* Schetch per pilotare fino a 8 relais indicando, per ognuno di essi, che azione 
* fare e quando.
* Per ora viene supportato il pilotaggio dei relais tramite I2C solamente (senza libreria, pilotando direttamente Wire).
* Nella prossima release si potranno pilotare anche relais collegati direttamente alle porte digitali.
* 
* Previsto anche l'inserimento opzionale di un sensore DHT11 per controllare la temperatura
* o l'umidità.
* Le azioni previste sono:
* 
* ActionType:  0 = do nothing
*              1 = ON when time (HH:MM) between Data[0]:Data[1] and Data[2]:Data[3] every day
*              2 = ON every Data[2] minutes (max 120) with duration Data[2] minutes (max 120) starting at Data[0]:Data[1]
*              3 = ON every Data[2] hours (max 12) with duration Data[3] minutes (max 120) starting at Data[0]:Data[1]
*              4 = ON when time (HH:MM) between Data[0]:Data[1] and Data[2]:Data[3] only if day of week is active in Data[4] (bitmask)
*              5 = ON every Data[2] minutes (max 120) with duration Data[3] minutes (max 120) starting at Data[0]:Data[1] only if day of week is active in Data[4] (bitmask)
*              6 = ON every Data[2] hours (max 12) with duration Data[3] minutes (max 120) starting at Data[0]:Data[1] only if day of week is active in Data[4] (bitmask)
*              IF DHT11 INSTALLED:
*              7 = ON when Temp below Data[0] , OFF when Temp above Data[1] 
*                  E.g:  Data[0] : 5, Data[1] : 10
*                  ON when Temp is below 5° and OFF when raises to +10°
*              8 = ON when Temp over Data[0] , OFF when Temp below Data[1] 
*              9 = ON when Humidity below Data[0], OFF when Humidity above Data[1] 
*             10 = ON when Humidity above Data[0], OFF when Humidity below Data[1] 
*/

#include "Arduino.h"
/* Precompiler defines */
/* if you want debug, uncomment next row */
// #define DEBUG_ON 1
/* ************************************* */

/* if you want relais Action types 7,8 (temp) and 9,10 (humidity) uncomment next line */
#define DHT11_PRESENT 1
/* ************************************* */

/* Number of relaises to drive */
#define NUM_RELAISES 3
/* If relaises connected via I2C define address and presence */
// #define I2C_RELAISES 1
/* ********************************************************* */

#ifdef I2C_RELAISES
#define RELAIS_ADDRESS 0x03
#else
static const uint8_t RELAIS_PINS[NUM_RELAISES] = {A1, A2, A3};
#endif

#include <Keypad.h>
#include <LiquidCrystal_PCF8574.h>
#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>

#ifdef DHT11_PRESENT

#include <SimpleDHT.h>
#define PIN_DHT11 10
#define MAX_ACTION 10
#else
#define MAX_ACTION 6
#endif

#define LOOPING 0
#define IN_MENU 1

#define MANUAL 1
#define AUTO 2

#define ON 0
#define OFF 1
#define DO_NOTHING 2

#define BACKLIGHT_ON 1
#define BACKLIGHT_OFF 0
#ifdef DHT11_PRESENT
#define LCD_ADDRESS 0x27
#else
#define LCD_ADDRESS 0x3F
#endif
#define ANALOG_PORT A0

#define LCD_ROWS 2
#define LCD_COLUMNS 16

#define MY_TAG 30576

// how many msecs stay on menu after last keypress
#define MENU_TIMEOUT 20000L
// Declariation of amount of time the display remains on (msecs)
#define TIMER_DISPLAY 60000L
// Declaration of every how many msecs we get the temperature (warning: the DHT11 permits only a read every 2 seconds!)
#define TIMER_TEMP 5000L
// Declaration of: every 1000 msec refresh display
#define TIMER_SECS 1000L

#define BACKLIGHT_ON 1
#define BACKLIGHT_OFF 0

const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns
/* Constants strings to minimize memory used */
const char *RELAIS_PROMPT_1 = "R:%d T:%d On:%02d:%02d";
const char *RELAIS_PROMPT_2 = "       Off:%02d:%02d";
const char *RELAIS_PROMPT_3 = "Pe.%3dm Dur.%3dm";
const char *RELAIS_PROMPT_4 = "Pe.%2dh Dur.%3dm";
const char *RELAIS_PROMPT_5 = "R:%d Ora Inizio?";
const char *RELAIS_PROMPT_6 = "R:%d Durata (m)?";
const char *RELAIS_PROMPT_7 = "R:%d Periodo (%c)";
const char *RELAIS_PROMPT_8 = "N. Relais 0=fine";
const char *RELAIS_PROMPT_9 = "R:%d On:%c%02d%cC";
const char *RELAIS_PROMPT_10 = "T:%d Off:%c%02d%cC";
const char *RELAIS_PROMPT_11 = "R:%d On:%c%02d%c H";
const char *RELAIS_PROMPT_12 = "T:%d Off:%c%02d%c H";

const char *BLANKROW = "                ";
/* Variables */
/*
 * ReleStruct: 
 */
struct ReleStruct
{
  byte ICON;
  byte IO_PORT; // to do: used if no I2C
  byte ActionType;
  byte CurrentStatus;
  byte Data[5];
};
struct Config
{
  int TAG;                         // integer to know if I saved before on eeprom
  byte STATUS;                     // RUNNING, IN_MENU, etc ...
  byte MODE;                       // MANUAL, AUTO
  ReleStruct Relais[NUM_RELAISES]; // NUM_RELAISES relais
};

char hexaKeys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};
byte colPins[ROWS] = {5, 4, 3, 2}; //connect to the row pinouts of the keypad
byte rowPins[COLS] = {9, 8, 7, 6}; //connect to the column pinouts of the keypad

DateTime now;
Config MyConfig;

uint8_t lastSecond = 0;
uint32_t last_check = 0;
#ifdef DHT11_PRESENT
boolean TempOrHumidity = false;
#endif

int APERTO[8] = {
    0B0000100,
    0B0000100,
    0B0001110,
    0B0011111,
    0B0010011,
    0B0010011,
    0B0011111,
    0B0001110};
#define GOCCIA_APERTO_CHAR 0x01
int CHIUSO[8] = {
    0B0000100,
    0B0000100,
    0B0001010,
    0B0010001,
    0B0010001,
    0B0010001,
    0B0010001,
    0B0001110};
#define GOCCIA_CHIUSO_CHAR 0x03
int LAMPADA_ON[8] = {
    0B0001110,
    0B0011111,
    0B0011111,
    0B0011111,
    0B0011111,
    0B0001110,
    0B0001010,
    0B0001110};
#define LAMPADA_ON_CHAR 0x02
int LAMPADA_OFF[8] = {
    0B0001110,
    0B0010001,
    0B0010001,
    0B0010001,
    0B0010001,
    0B0001110,
    0B0001010,
    0B0001110};
#define LAMPADA_OFF_CHAR 0x04
int GIORNO_ON[8] = {
    0B0000100,
    0B0001110,
    0B0011111,
    0B0011111,
    0B0011111,
    0B0011111,
    0B0001110,
    0B0000100};
#define GIORNO_ON_CHAR 0x05
int GIORNO_OFF[8] = {
    0B0000000,
    0B0000000,
    0B0000000,
    0B0011111,
    0B0011111,
    0B0000000,
    0B0000000,
    0B0000000};
#define GIORNO_OFF_CHAR 0x06

#ifdef DHT11_PRESENT
int GRADI[8] = {
    0x06,
    0x09,
    0x09,
    0x06,
    0x00,
    0x00,
    0x00,
    0x00,
};
#define GRADI_CHAR 0x07
#endif

/* Timers */
unsigned long timeStartDisplay = 0L;
unsigned long timeStartMenu = 0L;
unsigned long timeStartTemp = 0L;
unsigned long timeSeconds = 0L;
unsigned long currentMillis = 0L;

byte bRelaysStatus = 0b11111111;
byte bAllOff = 0b11111111;

#ifdef DHT11_PRESENT
// Variabili lettura temperatura DHT11
byte temperature = 0;
byte humidity = 0;
byte humidity2 = 0;
byte temperature2 = 0;
byte data[40] = {0};
#endif

/* OBJECTS */
RTC_DS1307 RTC;
Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);
LiquidCrystal_PCF8574 lcd(LCD_ADDRESS);
#ifdef DHT11_PRESENT
SimpleDHT11 dht11;
#endif

/* Function prototypes */
void DisplayAll();
void DisplayTime(int col = 0, int row = 0);
void DisplayStatus(int col = LCD_COLUMNS - NUM_RELAISES, int row = 1);
void DisplayWeekData(int relais, int waittime = 0);
void DisplayRelayData(int v);
void DisplayMenu();
void ConfigureRelayData(int v);
void flipProgramStatus();
int getInputInt(String prompt, int defval = -1, int minval = 0, int maxval = 240);
DateTime getInputTime(String prompt, DateTime defval = DateTime(2000, 1, 1, 0, 0, 0));
void getInputWeek(int v);
void displayMessage(String mess);
byte CheckTime(int R);
byte CheckPeriod(int R);
void RelaisON(byte index);
void RelaisOFF(byte index);
void ToggleWeekBit(int relais, int index);
void ToggleRelais(byte index);
#ifdef I2C_RELAISES
void SetupRelays(byte address = RELAIS_ADDRESS);
void DriveRelays(byte address = RELAIS_ADDRESS);
#else
void SetupRelays();
void DriveRelays();
#endif
void WriteRelays(byte address, byte data);
void MCP_Write(byte MCPaddress, byte MCPregister, byte MCPdata);
#ifdef DHT11_PRESENT
byte CheckTempHumidity(int R);
boolean ReadTemp();
void DisplayTemp();
byte b2byte(byte data[8]);
#endif

/* ------------------- */

void setup()
{
  int error;

  pinMode(ANALOG_PORT, INPUT);

#ifdef DEBUG_ON
  Serial.begin(9600);
#endif
  Wire.begin();
  Wire.beginTransmission(LCD_ADDRESS);
  error = Wire.endTransmission();
  if (error == 0)
  {
    lcd.begin(LCD_COLUMNS, LCD_ROWS); // initialize the lcd
  }
#ifdef DEBUG_ON
  else
  {
    Serial.println("No LCD");
  }
#endif
  lcd.init();
  lcd.setBacklight(BACKLIGHT_ON);
  lcd.createChar(GOCCIA_CHIUSO_CHAR, CHIUSO);
  lcd.createChar(GOCCIA_APERTO_CHAR, APERTO);
  lcd.createChar(LAMPADA_OFF_CHAR, LAMPADA_OFF);
  lcd.createChar(LAMPADA_ON_CHAR, LAMPADA_ON);
  lcd.createChar(GIORNO_OFF_CHAR, GIORNO_OFF);
  lcd.createChar(GIORNO_ON_CHAR, GIORNO_ON);
#ifdef DHT11_PRESENT
  lcd.createChar(GRADI_CHAR, GRADI);
#endif

  RTC.begin();
  if (!RTC.isrunning())
  {
    RTC.adjust(DateTime(__DATE__, __TIME__));
  }
#ifdef DEBUG_ON
  Serial.println("Set Clock !");
  RTC.adjust(DateTime(__DATE__, __TIME__));
  Serial.println("Read Config");
#endif
  EEPROM.get(0, MyConfig);
#ifdef DEBUG_ON
  Serial.print("TAG:");
  Serial.println((int)MyConfig.TAG);
#endif
  if ((int)MyConfig.TAG != (int)MY_TAG)
  { // if TAG not equal to mine, set defaults and write
#ifdef DEBUG_ON
    Serial.println("No memory: set defaults");
#endif
    MyConfig.TAG = MY_TAG;
    MyConfig.STATUS = LOOPING;
    MyConfig.MODE = AUTO;
    for (int i = 0; i < NUM_RELAISES; i++)
    {
      MyConfig.Relais[i].ActionType = 0;
      MyConfig.Relais[i].CurrentStatus = OFF;
      MyConfig.Relais[i].IO_PORT = i + 1;
      for (int j = 0; j < 5; j++)
      {
        MyConfig.Relais[i].Data[j] = 0;
      }
    }
    EEPROM.put(0, MyConfig);
  }
  else
  {
#ifdef DEBUG_ON
    Serial.println("Got values from memory!");
#endif
  }
  SetupRelays();

  currentMillis = millis();
  now = RTC.now();
  timeStartTemp = currentMillis;
  timeSeconds = currentMillis;
  timeStartDisplay = currentMillis;
}

void loop()
{
  char lettera;
  DateTime dt;
  int v;
  int NumRel;
  char ch;

  currentMillis = millis();
  now = RTC.now();

  if (currentMillis > (timeStartDisplay + TIMER_DISPLAY))
  {
    lcd.setBacklight(BACKLIGHT_OFF);
  }
  lettera = customKeypad.getKey();
  if (now.second() != lastSecond)
  {
    lastSecond = now.second();
    DisplayTime(0, 0);
    DisplayStatus();
  }
  if (lettera)
  {
    lcd.setBacklight(BACKLIGHT_ON);
    timeStartDisplay = currentMillis;
    if (MyConfig.STATUS == MANUAL)
    {
      switch (lettera)
      {
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
        NumRel = lettera - 48;
        if (NumRel <= NUM_RELAISES)
        {
          if (MyConfig.Relais[NumRel - 1].ActionType > 0)
            ToggleRelais(NumRel);
        }
        break;
      case 'A':
        flipProgramStatus();
        break;
      }
    }
    else
    {
      switch (lettera)
      {
      case 'A':
        flipProgramStatus();
        break;
      case 'D': // menu
        DisplayMenu();
        do
        {
          ch = customKeypad.getKey();
          if (ch)
          {
            timeStartDisplay = currentMillis;
            switch (ch)
            {
            case 'A':
              do
              {
                v = getInputInt(RELAIS_PROMPT_8, 0, 0, NUM_RELAISES);
                if (v >= 1 && v <= NUM_RELAISES)
                {
                  DisplayRelayData(v);
                }
              } while (v > 0);
              DisplayMenu();
              break;
            case 'B':
              do
              {
                v = getInputInt(RELAIS_PROMPT_8, 0, 0, NUM_RELAISES);
                if (v >= 1 && v <= NUM_RELAISES)
                {
                  ConfigureRelayData(v);
                }
              } while (v > 0);
              DisplayMenu();
              break;
            case 'C':
            {
              int gg = now.day(),
                  mm = now.month(),
                  aa = now.year(),
                  hh = now.hour(),
                  mi = now.minute();
              gg = getInputInt("Set Giorno", gg, 1, 31);
              mm = getInputInt("Set Mese", mm, 1, 12);
              aa = getInputInt("Set Anno", aa, 2020, 2040);
              hh = getInputInt("Set Ora", hh, 0, 23);
              mi = getInputInt("Set Minuti", mi, 0, 59);
              RTC.adjust(DateTime(aa, mm, gg, hh, mi, 0));
            }
              DisplayMenu();
              break;
            }
          }
        } while (ch != 'D');
        lcd.clear();
      }
    }
  }
  else
  {
    if (now.unixtime() < last_check)
    {
      last_check = now.unixtime();
    }
    if ((now.unixtime() > last_check + 5) && (MyConfig.STATUS == AUTO))
    {

      last_check = now.unixtime();

#ifdef DHT11_PRESENT
      if (TempOrHumidity == false)
        TempOrHumidity = true;
      else
        TempOrHumidity = false;
      DisplayTemp();
#endif

      for (int i = 0; i < NUM_RELAISES; i++)
      {
        switch (MyConfig.Relais[i].ActionType)
        {
        case 0:
          break;
        case 1:
        case 4:
          if (CheckTime(i) == ON)
            RelaisON(i + 1);
          else
            RelaisOFF(i + 1);
          break;
        case 2:
        case 3:
        case 5:
        case 6:
          if (CheckPeriod(i) == ON)
            RelaisON(i + 1);
          else
            RelaisOFF(i + 1);
          break;
#ifdef DHT11_PRESENT
        case 7:
        case 8:
        case 9:
        case 10:
        {
          int vv = CheckTempHumidity(i);
#ifdef DEBUG_ON
          Serial.print("Ret:");
          Serial.println(vv);
#endif
          switch (vv)
          {
          case ON:
            RelaisON(i + 1);
            break;
          case OFF:
            RelaisOFF(i + 1);
            break;
          case DO_NOTHING:
            break;
          }
        }
        break;
#endif
        }
      }
      DriveRelays();
    }
  }
}

/* ******************* FUNCTIONS *********************** */
void flipProgramStatus()
{
  if (MyConfig.STATUS == AUTO)
  {
    MyConfig.STATUS = MANUAL;
  }
  else
  {
    MyConfig.STATUS = AUTO;
  }
}
void DisplayAll()
{
  DisplayTime();
  DisplayStatus();
}
void DisplayTime(int col, int row)
{
  lcd.setCursor(col, row);
  lcd.print((now.timestamp(DateTime::TIMESTAMP_TIME)).substring(0, 5));
  lcd.print((MyConfig.STATUS == AUTO) ? " AUTOMATICO" : "  MANUALE  ");
}

void DisplayStatus(int col, int row)
{
  char buf[16];
  lcd.setCursor(col, row);
  for (int i = 0; i < NUM_RELAISES; i++)
  {
    if (MyConfig.Relais[i].ActionType > 0)
    {
      sprintf(buf, "%c", ((MyConfig.Relais[i].CurrentStatus == OFF)) ? MyConfig.Relais[i].ICON + 2 : MyConfig.Relais[i].ICON);
    }
    else
    {
      sprintf(buf, "-");
    }
    lcd.print(buf);
  }
}

void DisplayWeekData(int relais, int waittime)
{
  char buf[16];
  if (waittime > 0)
  {
    delay(waittime);
    lcd.setCursor(7, 0);
    lcd.print("  ");
    lcd.setCursor(7, 1);
    lcd.print("  ");
  }
  lcd.setCursor(9, 0);
  lcd.print("DLMMGVS");
  lcd.setCursor(9, 1);
  for (int i = 0; i < 7; i++)
  {
    sprintf(buf, "%c", (bit_is_set(MyConfig.Relais[relais].Data[4], i)) ? GIORNO_ON_CHAR : GIORNO_OFF_CHAR);
    lcd.print(buf);
  }
}

void DisplayRelayData(int v)
{
  char buf[32];
  lcd.clear();
  lcd.setCursor(0, 0);
  switch (MyConfig.Relais[v - 1].ActionType)
  {
  case 0:
    sprintf(buf, "R:%d Disattivo", v);
    lcd.print(buf);
    break;
  case 1:
  case 4:
    sprintf(buf, RELAIS_PROMPT_1, v, MyConfig.Relais[v - 1].ActionType, MyConfig.Relais[v - 1].Data[0], MyConfig.Relais[v - 1].Data[1]);
    lcd.print(buf);
    lcd.setCursor(0, 1);
    sprintf(buf, RELAIS_PROMPT_2, MyConfig.Relais[v - 1].Data[2], MyConfig.Relais[v - 1].Data[3]);
    lcd.print(buf);
    if (MyConfig.Relais[v - 1].ActionType == 4)
    {
      DisplayWeekData(v - 1, 5000);
    }
    break;
  case 2:
  case 5:
    sprintf(buf, RELAIS_PROMPT_1, v, MyConfig.Relais[v - 1].ActionType, MyConfig.Relais[v - 1].Data[0], MyConfig.Relais[v - 1].Data[1]);
    lcd.print(buf);
    lcd.setCursor(0, 1);
    sprintf(buf, RELAIS_PROMPT_3, MyConfig.Relais[v - 1].Data[2], MyConfig.Relais[v - 1].Data[3]);
    lcd.print(buf);
    if (MyConfig.Relais[v - 1].ActionType == 5)
    {
      DisplayWeekData(v - 1, 5000);
    }
    break;
  case 3:
  case 6:
    sprintf(buf, RELAIS_PROMPT_1, v, MyConfig.Relais[v - 1].ActionType, MyConfig.Relais[v - 1].Data[0], MyConfig.Relais[v - 1].Data[1]);
    lcd.print(buf);
    lcd.setCursor(0, 1);
    sprintf(buf, RELAIS_PROMPT_4, MyConfig.Relais[v - 1].Data[2], MyConfig.Relais[v - 1].Data[3]);
    lcd.print(buf);
    if (MyConfig.Relais[v - 1].ActionType == 6)
    {
      DisplayWeekData(v - 1, 5000);
    }
    break;
  case 7:
  case 8:
    sprintf(buf, RELAIS_PROMPT_9,v,(MyConfig.Relais[v - 1].ActionType % 2 == 0)?'>':'<',
                                    MyConfig.Relais[v - 1].Data[0],GRADI_CHAR);
    lcd.print(buf);
    lcd.setCursor(0, 1);
    sprintf(buf, RELAIS_PROMPT_10,MyConfig.Relais[v - 1].ActionType,(MyConfig.Relais[v - 1].ActionType % 2 == 0)?'<':'>',
                                    MyConfig.Relais[v - 1].Data[1],GRADI_CHAR);
    lcd.print(buf);
    break;
  case 9:
  case 10:
    sprintf(buf, RELAIS_PROMPT_11,v,(MyConfig.Relais[v - 1].ActionType % 2 == 0)?'>':'<',
                                    MyConfig.Relais[v - 1].Data[0],'%');
    lcd.print(buf);
    lcd.setCursor(0, 1);
    sprintf(buf, RELAIS_PROMPT_12,MyConfig.Relais[v - 1].ActionType,(MyConfig.Relais[v - 1].ActionType % 2 == 0)?'<':'>',
                                    MyConfig.Relais[v - 1].Data[1],'%');
    lcd.print(buf);
    break;
  }
  delay(5000);
}

void DisplayMenu()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("A:Vedi B=Config.");
  lcd.setCursor(0, 1);
  lcd.print("C:Set Ora D:Fine");
}
void ConfigureRelayData(int v)
{
  char buf[32];
  int dato;
  int action;
  DateTime dt;
  lcd.clear();
  lcd.setCursor(0, 0);
  do
  {
    sprintf(buf, "R:%d Tipo ?", v);
    action = getInputInt(buf, (int)MyConfig.Relais[v - 1].ActionType, 0, MAX_ACTION);
    if (action > 0)
    {
      sprintf(buf, "Icona? 1=%c 2=%c", GOCCIA_CHIUSO_CHAR, LAMPADA_OFF_CHAR);
      dato = getInputInt(buf, (int)MyConfig.Relais[v - 1].ICON, 1, 2);
      MyConfig.Relais[v - 1].ICON = dato;
    }
    switch (action)
    {
    case 0:
      break;
    case 1:
    case 4:
      sprintf(buf, "R:%d Ora Acceso?", v);
      dt = getInputTime(buf, DateTime(2000, 1, 1, MyConfig.Relais[v - 1].Data[0], MyConfig.Relais[v - 1].Data[1], 0));
      MyConfig.Relais[v - 1].Data[0] = dt.hour();
      MyConfig.Relais[v - 1].Data[1] = dt.minute();
      sprintf(buf, "R:%d Ora Spento?", v);
      dt = getInputTime(buf, DateTime(2000, 1, 1, MyConfig.Relais[v - 1].Data[2], MyConfig.Relais[v - 1].Data[3], 0));
      MyConfig.Relais[v - 1].Data[2] = dt.hour();
      MyConfig.Relais[v - 1].Data[3] = dt.minute();
      if (action == 4)
      {
        getInputWeek(v - 1);
      }
      else
      {
        MyConfig.Relais[v - 1].Data[4] = 0xFF;
      }
      break;
    case 2:
    case 5:
      sprintf(buf, RELAIS_PROMPT_5, v);
      dt = getInputTime(buf, DateTime(2000, 1, 1, MyConfig.Relais[v - 1].Data[0], MyConfig.Relais[v - 1].Data[1], 0));
      MyConfig.Relais[v - 1].Data[0] = dt.hour();
      MyConfig.Relais[v - 1].Data[1] = dt.minute();
      sprintf(buf, RELAIS_PROMPT_7, v, 'm');
      dato = getInputInt(buf, (int)MyConfig.Relais[v - 1].Data[2], 5, 240);
      MyConfig.Relais[v - 1].Data[2] = dato;
      sprintf(buf, RELAIS_PROMPT_6, v);
      dato = getInputInt(buf, (int)MyConfig.Relais[v - 1].Data[3], 5, dato);
      MyConfig.Relais[v - 1].Data[3] = dato;
      if (action == 4)
      {
        getInputWeek(v - 1);
      }
      else
      {
        MyConfig.Relais[v - 1].Data[4] = 0xFF;
      }
      break;
    case 3:
    case 6:
      sprintf(buf, RELAIS_PROMPT_5, v);
      dt = getInputTime(buf, DateTime(2000, 1, 1, MyConfig.Relais[v - 1].Data[0], MyConfig.Relais[v - 1].Data[1], 0));
      MyConfig.Relais[v - 1].Data[0] = dt.hour();
      MyConfig.Relais[v - 1].Data[1] = dt.minute();
      sprintf(buf, RELAIS_PROMPT_7, v, 'h');
      dato = getInputInt(buf, (int)MyConfig.Relais[v - 1].Data[2], 1, 18);
      MyConfig.Relais[v - 1].Data[2] = dato;
      sprintf(buf, RELAIS_PROMPT_6, v);
      dato = getInputInt(buf, (int)MyConfig.Relais[v - 1].Data[3], 5, ((dato * 60) > 240) ? 120 : dato * 60);
      MyConfig.Relais[v - 1].Data[3] = dato;
      if (action == 4)
      {
        getInputWeek(v - 1);
      }
      else
      {
        MyConfig.Relais[v - 1].Data[4] = 0xFF;
      }
      break;
#ifdef DHT11_PRESENT
    case 7:
      sprintf(buf, "On se Temp.<%cC", GRADI_CHAR);
      dato = getInputInt(buf, (int)MyConfig.Relais[v - 1].Data[0], 0, 60);
      MyConfig.Relais[v - 1].Data[0] = dato;
      sprintf(buf, "Off se Temp>%cC", GRADI_CHAR);
      dato = getInputInt(buf, (int)MyConfig.Relais[v - 1].Data[1], dato, 60);
      MyConfig.Relais[v - 1].Data[1] = dato;
      break;
    case 8:
      sprintf(buf, "On se Temp.>%cC", GRADI_CHAR);
      dato = getInputInt(buf, (int)MyConfig.Relais[v - 1].Data[0], 0, 60);
      MyConfig.Relais[v - 1].Data[0] = dato;
      sprintf(buf, "Off se Temp<%cC", GRADI_CHAR);
      dato = getInputInt(buf, (int)MyConfig.Relais[v - 1].Data[1], 0, dato);
      MyConfig.Relais[v - 1].Data[1] = dato;
      break;
    case 9:
      sprintf(buf, "On se Umid.< %c", '%');
      dato = getInputInt(buf, (int)MyConfig.Relais[v - 1].Data[0], 0, 100);
      MyConfig.Relais[v - 1].Data[0] = dato;
      sprintf(buf, "Off se Umid.> %c", '%');
      dato = getInputInt(buf, (int)MyConfig.Relais[v - 1].Data[1], dato, 100);
      MyConfig.Relais[v - 1].Data[1] = dato;
      break;
    case 10:
      sprintf(buf, "On se Umid.> %c", '%');
      dato = getInputInt(buf, (int)MyConfig.Relais[v - 1].Data[0], 0, 100);
      MyConfig.Relais[v - 1].Data[0] = dato;
      sprintf(buf, "Off se Umid.< %c", '%');
      dato = getInputInt(buf, (int)MyConfig.Relais[v - 1].Data[1], 0, dato);
      MyConfig.Relais[v - 1].Data[1] = dato;
      break;
#endif
    default:
      break;
    }
  } while (action > MAX_ACTION);
  MyConfig.Relais[v - 1].ActionType = action;
  EEPROM.put(0, MyConfig);
}

void getInputWeek(int relais)
{
  char lettera;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("1-7:set");
  lcd.setCursor(0, 1);
  lcd.print("A:Save");
  DisplayWeekData(relais);
  do
  {
    lettera = customKeypad.getKey();
    if (lettera)
    {
      switch (lettera)
      {
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
        ToggleWeekBit(relais, lettera - 49);
        break;
      case 'A':
        break;
      }
      if (lettera == 'A')
      {
        EEPROM.put(0, MyConfig);
        break;
      }
      DisplayWeekData(relais);
    }
  } while (true); // until not a correct number
  lcd.clear();
  lcd.noBlink();
  lcd.noCursor();
}

int getInputInt(String prompt, int defval, int minval, int maxval)
{
  char lettera;
  int pos = 0, retVal;
  char sTemp[16];
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(prompt);
  if (defval != -1)
  {
    itoa(defval, sTemp, 10);
    lcd.setCursor(0, 1);
    lcd.print(sTemp);
  }
  do
  {
    lcd.setCursor(pos, 1);
    lcd.blink();
    lettera = customKeypad.getKey();
    if ((lettera >= '0') && (lettera <= '9'))
    {
      lcd.print(lettera);
      sTemp[pos] = lettera;
      pos++;
    }
    if (lettera == 'C') // Clear
    {
      pos = 0;
      lcd.setCursor(pos, 1);
      lcd.print(BLANKROW);
      memset(sTemp, 0, 16);
    }
    if (lettera == '#') //Enter
    {
      retVal = String(sTemp).toInt();
      if (retVal < minval)
        displayMessage("Min." + String(minval));
      else if (retVal > maxval)
        displayMessage("Max." + String(maxval));
      else
        break;
    }
  } while (true); // until not a correct number
  lcd.clear();
  lcd.noBlink();
  lcd.noCursor();
  return retVal;
}
DateTime getInputTime(String prompt, DateTime defval)
{

  char lettera;
  int pos = 0;
  char sTemph[16];
  char sTempm[16];
  int h, m;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(prompt);
  lcd.setCursor(2, 1);
  lcd.print(":");

  if (defval != DateTime(2000, 1, 1, 0, 0, 0))
  {
    char buffer[16];
    sprintf(buffer, "%02d:%02d", defval.hour(), defval.minute());
    lcd.setCursor(0, 1);
    lcd.print(buffer);
    pos = 0;
    itoa(defval.hour(), sTemph, 10);
    itoa(defval.minute(), sTempm, 10);
  }
  /* ore ... 2 cifre */
  do
  {
    do
    {
      lcd.setCursor(pos, 1);
      lcd.blink();
      lettera = customKeypad.getKey();
      if ((lettera >= '0') && (lettera <= '9'))
      {
        lcd.print(lettera);
        if (pos < 2)
          sTemph[pos] = lettera;
        else
          sTempm[pos - 3] = lettera;
        switch (pos)
        {
        case 1:
          pos++;
        case 0:
        case 3:
          pos++;
        }
      }
      if (lettera == '*')
      {
        switch (pos)
        {
        case 3:
          pos--;
        case 1:
        case 4:
          pos--;
        }
      }
      if (lettera == 'C')
      {
        pos = 0;
        lcd.setCursor(pos, 1);
        lcd.print(BLANKROW);
        lcd.setCursor(2, 1);
        lcd.print(":");
        memset(sTemph, 0, 16);
        memset(sTempm, 0, 16);
      }
    } while (lettera != '#');
    h = String(sTemph).toInt();
    m = String(sTempm).toInt();
    if (h >= 24)
      displayMessage("Err.Ora:" + String(h));
    else if (m >= 60)
      displayMessage("Err.Min:" + String(m));
    else
      break;
  } while (true);
  lcd.clear();
  lcd.noBlink();
  lcd.noCursor();
  return DateTime(2000, 1, 1, h, m, 0);
}
void displayMessage(String mess)
{
  lcd.setCursor(6, 1);
  lcd.print(mess);
  delay(1000);
  lcd.setCursor(6, 1);
  lcd.print("          ");
}

void RelaisON(byte index)
{
  if (index > 0 && index <= NUM_RELAISES)
  {
    MyConfig.Relais[index - 1].CurrentStatus = ON;
  }
}
void RelaisOFF(byte index)
{
  if (index > 0 && index <= NUM_RELAISES)
  {
    MyConfig.Relais[index - 1].CurrentStatus = OFF;
  }
}
void ToggleRelais(byte index)
{
  if (index > 0 && index <= NUM_RELAISES)
  {
    if (MyConfig.Relais[index - 1].CurrentStatus == OFF)
    {
      MyConfig.Relais[index - 1].CurrentStatus = ON;
    }
    else
    {
      MyConfig.Relais[index - 1].CurrentStatus = OFF;
    }
    DriveRelays();
  }
}
byte CheckPeriod(int R)
{
  DateTime tStartTime, tStartPeriod, tEndPeriod;
  TimeSpan tSpanPeriod, tSpanDuration;

  if (bit_is_clear(MyConfig.Relais[R].Data[4], now.dayOfTheWeek()))
  {
    return OFF;
  }
  switch (MyConfig.Relais[R].ActionType)
  {
  case 2:
  case 5:
    tSpanPeriod = TimeSpan(0, 0, (int)MyConfig.Relais[R].Data[2], 0); //Period in minutes
    break;
  case 3:
  case 6:
    tSpanPeriod = TimeSpan(0, (int)MyConfig.Relais[R].Data[2], 0, 0); //Period in hours
    break;
  default:
    return OFF; // only for ActionType 2 and 3
  }
  tSpanDuration = TimeSpan(0, 0, (int)MyConfig.Relais[R].Data[3], 0); // Duration in minutes
  tStartTime = tStartPeriod = DateTime(now.year(), now.month(), now.day(), MyConfig.Relais[R].Data[0], MyConfig.Relais[R].Data[1], 0);
  if (tStartTime > now)
    return OFF;
  tEndPeriod = tStartPeriod + tSpanDuration;
  if (tEndPeriod > now)
    return ON;
  while (tEndPeriod < now)
  {
    tEndPeriod = tEndPeriod + tSpanPeriod;
  }
  tStartPeriod = tEndPeriod - tSpanDuration;

#ifdef DEBUG_ON
  Serial.print("Relais ");
  Serial.print(R);
  Serial.print(" Next start: ");
  Serial.print(tStartPeriod.timestamp());
  Serial.print(" Next end: ");
  Serial.println(tEndPeriod.timestamp());
#endif

  if ((now > tStartPeriod) && (now < tEndPeriod))
    return ON;
  return OFF;
}
byte CheckTime(int R)
{
  DateTime tStartTime, tEndTime;

#ifdef DEBUG_ON
  Serial.println(now.dayOfTheWeek());
#endif
  if (bit_is_clear(MyConfig.Relais[R].Data[4], now.dayOfTheWeek()))
  {
    return OFF;
  }

  tStartTime = DateTime(now.year(), now.month(), now.day(), MyConfig.Relais[R].Data[0], MyConfig.Relais[R].Data[1], 0);
  tEndTime = DateTime(now.year(), now.month(), now.day(), MyConfig.Relais[R].Data[2], MyConfig.Relais[R].Data[3], 0);

#ifdef DEBUG_ON
  Serial.print("Relais ");
  Serial.print(R);
  Serial.print(" Next start: ");
  Serial.print(tStartTime.timestamp());
  Serial.print(" Next end: ");
  Serial.println(tEndTime.timestamp());
#endif

  if (tEndTime > tStartTime)
  {
    if ((now > tStartTime) && (now < tEndTime)) // during day
      return ON;
  }
  else
  {
    if ((now > tStartTime) || (now < tEndTime)) // crossing midnight
      return ON;
  }
  return OFF;
}

#ifdef I2C_RELAISES
void SetupRelays(byte addr)
{
  bRelaysStatus = bAllOff;
  MCP_Write(addr, 0x00, 0b00000000);    //  set all pins to output
  MCP_Write(addr, 0x12, bRelaysStatus); //  set all outputs to off
}
void DriveRelays(byte addr)
{
  bRelaysStatus = bAllOff;
  for (int i = 0; i < NUM_RELAISES; i++)
  {
    if (MyConfig.Relais[i].CurrentStatus == OFF)
    {
      bitSet(bRelaysStatus, i);
    }
    else
    {
      bitClear(bRelaysStatus, i);
    }
  }
  MCP_Write(addr, 0x12, bRelaysStatus); //  set all outputs, for I2C relais
}
#else
void SetupRelays()
{
  for (int i = 0; i < NUM_RELAISES; i++)
  {
    pinMode(RELAIS_PINS[i], OUTPUT);
    digitalWrite(RELAIS_PINS[i], HIGH);
  }
}
void DriveRelays()
{
  for (int i = 0; i < NUM_RELAISES; i++)
  {
    digitalWrite(RELAIS_PINS[i], (MyConfig.Relais[i].CurrentStatus == OFF) ? HIGH : LOW);
  }
}
#endif
void ToggleWeekBit(int relais, int i)
{
#ifdef DEBUG_ON
  char buf[16];
  sprintf(buf, "R:%d B:%x", relais, MyConfig.Relais[relais].Data[4]);
  Serial.println(buf);
#endif
  if (bit_is_clear(MyConfig.Relais[relais].Data[4], i))
  {
    bitSet(MyConfig.Relais[relais].Data[4], i);
  }
  else
  {
    bitClear(MyConfig.Relais[relais].Data[4], i);
  }
#ifdef DEBUG_ON
  sprintf(buf, "R:%d A:%x", relais, MyConfig.Relais[relais].Data[4]);
  Serial.println(buf);
#endif
}

void WriteRelays(byte address, byte data)
{
  // Write data to relays
  // --------------------
  MCP_Write(address, 0x12, data);
}

void MCP_Write(byte MCPaddress, byte MCPregister, byte MCPdata)
{
  // I2C write routine
  // -----------------
  MCPaddress = MCPaddress + 0x20; // 0x20 is base address for MCP
  Wire.beginTransmission(MCPaddress);
  Wire.write(MCPregister);
  Wire.write(MCPdata);
  Wire.endTransmission();
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// End of MCP routines
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Begin of DHT11 routines
#ifdef DHT11_PRESENT

byte CheckTempHumidity(int R)
{
  byte ValueLimitLow;
  byte ValueLimitHigh;
  ValueLimitLow = MyConfig.Relais[R].Data[0];
  ValueLimitHigh = MyConfig.Relais[R].Data[1];
#ifdef DEBUG_ON
  Serial.print("Relais:");
  Serial.println(R);
  Serial.print("Action:");
  Serial.println(MyConfig.Relais[R].ActionType);
#endif
  if ((int)MyConfig.Relais[R].ActionType == 7)
  {
#ifdef DEBUG_ON
    Serial.print("Action:");
    Serial.println(MyConfig.Relais[R].ActionType);
    Serial.print("Low:");
    Serial.println(ValueLimitLow);
    Serial.print("High:");
    Serial.println(ValueLimitHigh);
    Serial.print("Temp:");
    Serial.println(temperature);
    Serial.print("Temp2:");
    Serial.println(temperature2);
#endif
    if (temperature2 > 127)
    {
      return ON; // Se sottozero, torna sempre ON !!
    }
    if ((int)temperature < (int)ValueLimitLow)
    {
      return ON;
    }
    else
    {
      if ((int)temperature >= (int)ValueLimitHigh)
      {
        return OFF;
      }
      else
      {
        return DO_NOTHING;
      }
    }
  }
  if ((int)MyConfig.Relais[R].ActionType == 8)
  {
    if (temperature2 > 127)
    {
      return OFF; // Se sottozero, torna sempre OFF !!
    }
    if (temperature < ValueLimitLow)
    {
      return OFF;
    }
    else
    {
      if (temperature >= ValueLimitHigh)
      {
        return ON;
      }
      else
      {
        return DO_NOTHING;
      }
    }
  }
  if ((int)MyConfig.Relais[R].ActionType == 9)
  {
    if (humidity < ValueLimitLow)
    {
      return ON;
    }
    else
    {
      if (humidity >= ValueLimitHigh)
      {
        return OFF;
      }
      else
      {
        return DO_NOTHING;
      }
    }
  }
  if ((int)MyConfig.Relais[R].ActionType == 10)
  {

    if (humidity < ValueLimitLow)
    {
      return OFF;
    }
    else
    {
      if (humidity >= ValueLimitHigh)
      {
        return ON;
      }
      else
      {
        return DO_NOTHING;
      }
    }
  }
  return DO_NOTHING;
}

boolean ReadTemp()
{
  if (!dht11.read(PIN_DHT11, &temperature, &humidity, data))
  {
    humidity2 = b2byte(data + 8);
    temperature2 = b2byte(data + 24);
    return true;
  }
  return false;
}

void DisplayTemp()
{
  char buffer[32];
  byte Sign = '+';
  if (ReadTemp())
  {
    if (temperature2 > 127)
    {
      Sign = '-';
    }
    lcd.setCursor(0, 1);
    if (TempOrHumidity == false)
      sprintf(buffer, "T%c%2d.%01d%c", Sign, temperature, temperature2 % 128, GRADI_CHAR);
    else
      sprintf(buffer, "H:%02d.%01d%s", humidity, humidity2, "%");
    lcd.print(buffer);
  }
  else
  {
    lcd.setCursor(0, 1);
    lcd.print("KO Temp");
  }
}

byte b2byte(byte data[8])
{
  byte v = 0;
  for (int i = 0; i < 8; i++)
  {
    v += data[i] << (7 - i);
  }
  return v;
}
// END of DHT11 routines
#endif