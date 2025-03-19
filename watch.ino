#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TM1637Display.h>

#define POTI_PIN      A0
#define BUTTON_PIN    4
#define light_sens    A1
#define red           8
#define blue          9
#define green         10
#define buzzer        3

#define HYSTERESE     10
#define MAX_POTI_VALUE 1000
#define DEBOUNCE_DELAY 50
#define wokeMax       20

#define CLK 5
#define DIO 6

LiquidCrystal_I2C lcd(0x27, 20, 4);
TM1637Display display(CLK, DIO);

// Einfache Ausgabe von Menü-Strings
void printStr(const char* const* table, uint8_t index) {
  lcd.print(table[index]);
}

#define CLEAR_LINE_20  "                    "

// Alle Menü-Texte
const char strZurueck[]       = "Zurueck";
const char strWecker[]        = "Wecker";
const char strEinstellungen[] = "Einstellungen";
const char strInfo[]          = "Info";
const char strStunden[]       = "Stunden";
const char strMinuten[]       = "Minuten";
const char strAutomLicht[]    = "Autom. Licht";
const char strLoudspeaker[]   = "Lautsprecher";
const char strVersion[]       = "Version: Alpha 1.0";
const char strCredits[]       = "Credits";
const char CREDITS_LINE1[]    = "Credits:";
const char CREDITS_LINE2[]    = "Made with help of:";
const char CREDITS_LINE3[]    = "- Myself :D and GPT";

const char* const menuItems[] = {
    strWecker,
    strEinstellungen,
    strInfo,
    strZurueck
};
const uint8_t menuLength = sizeof(menuItems) / sizeof(menuItems[0]);

const char* const subMenu_Wecker[] = {
    strStunden,
    strMinuten,
    strZurueck
};

const char* const subMenu_Einstellungen[] = {
    strAutomLicht,
    strLoudspeaker,
    strZurueck
};

const char* const subMenu_Info[] = {
    strVersion,
    strCredits,
    strZurueck
};

const char* const * const subMenus[] = {
    subMenu_Wecker,
    subMenu_Einstellungen,
    subMenu_Info
};

const uint8_t subMenuLengths[] = {
    sizeof(subMenu_Wecker)         / sizeof(subMenu_Wecker[0]),
    sizeof(subMenu_Einstellungen)  / sizeof(subMenu_Einstellungen[0]),
    sizeof(subMenu_Info)           / sizeof(subMenu_Info[0])
};

// Zustand und Flags
bool automatic_lightdimming = true;
bool isShowingCredits       = false;
bool isTimeReceived         = false;

uint8_t weckerStunden = 0;
uint8_t weckerMinuten = 0;
bool isSettingHours   = false;
bool isSettingMinutes = false;

uint8_t menuIndex         = 0;
uint8_t subMenuIndex      = 0;
int lastPotiValue         = -1;
bool isSubMenu            = false;
bool lastButtonState      = HIGH;
unsigned long lastDebounceTime = 0;
bool needsRedraw          = true;
bool lastWeckzeitDrawn    = false;
bool isMenu               = false;

uint8_t hour        = 0;
uint8_t minute      = 0;
String day          = "Monday";
int brightness      = 0;
uint8_t wokeCounter = 0;
int month           = 0;
int dayy            = 0;
int time_to_wecker  = 0;
bool loudspeakerOn  = false;

// Zeigt das Hauptmenü an
void displayMenu() {
    lcd.clear();
    for (uint8_t i = 0; i < 4; i++) {
        int itemIndex = menuIndex - 1 + i;
        lcd.setCursor(0, i);
        if (itemIndex < 0 || itemIndex >= menuLength) {
            lcd.print(CLEAR_LINE_20);
        } else {
            if (i == 1) lcd.print("> ");
            else lcd.print("  ");
            printStr(menuItems, itemIndex);
        }
    }
}

// Zeigt das Submenü an
void displaySubMenu() {
    lcd.clear();
    const char* const* thisSubMenu = subMenus[menuIndex];
    uint8_t subMenuLength = subMenuLengths[menuIndex];
    for (uint8_t i = 0; i < 4; i++) {
        int itemIndex = subMenuIndex - 1 + i;
        lcd.setCursor(0, i);
        if (itemIndex < 0 || itemIndex >= subMenuLength) {
            lcd.print(CLEAR_LINE_20);
        } else {
            if (i == 1) lcd.print("> ");
            else lcd.print("  ");
            printStr(thisSubMenu, itemIndex);
            if (menuIndex == 1 && itemIndex == 0) {
                lcd.setCursor(17, i);
                lcd.print(automatic_lightdimming ? "ON " : "OFF");
            } else if (menuIndex == 1 && itemIndex == 1) {
                lcd.setCursor(17, i);
                lcd.print(loudspeakerOn ? "ON " : "OFF");
            }
        }
    }
    // Zeigt die Weckzeit, wenn wir im Wecker-Menü sind
    if (menuIndex == 0) {
        lcd.setCursor(14, 0);
        lcd.print("    ");
        lcd.setCursor(14, 0);
        if (weckerStunden < 10) lcd.print('0');
        lcd.print(weckerStunden);
        lcd.print(':');
        if (weckerMinuten < 10) lcd.print('0');
        lcd.print(weckerMinuten);
    }
}

// Aktualisiert LCD und 7-Segment mit Uhrzeit
void displayTime(uint8_t hour, uint8_t minute, String day, bool automatic_lightdimming) {
    displayStandby(hour, minute, day);
    int num = hour * 100 + minute;
    if (num == 0) {
        display.showNumberDecEx(0, 0b11100000, true);
    } else {
        display.showNumberDecEx(num, 0b11100000, true);
    }
}

// Stellt die Hauptanzeige (Tag, Uhr, Datum) aufs LCD
void displayStandby(uint8_t hour, uint8_t minute, String day) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(day);
    if (day == "Friday") {
      lcd.print(" :)");
    }
    lcd.setCursor(8, 1);
    if (hour < 10) {
      lcd.print('0');
      lcd.print(hour);
    } else {
      lcd.print(hour);
    }
    lcd.print(':');
    if (minute < 10) {
      lcd.print('0');
      lcd.print(minute);
    } else {
      lcd.print(minute);
    }
    lcd.setCursor(0, 3);
    lcd.print("Date: ");
    if (dayy < 10) lcd.print("0");
    lcd.print(dayy);
    lcd.print(".");
    if (month < 10) lcd.print("0");
    lcd.print(month);
}

// Zeigt geänderte Stunden/Minuten im Wecker-Menü
void updateWeckzeit() {
    lcd.setCursor(14, 0);
    lcd.print("    ");
    lcd.setCursor(14, 0);
    if (weckerStunden < 10) lcd.print('0');
    lcd.print(weckerStunden);
    lcd.print(':');
    if (weckerMinuten < 10) lcd.print('0');
    lcd.print(weckerMinuten);
}

// Zeigt die Credits im Info-Menü
void displayCredits() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(CREDITS_LINE1);
    lcd.setCursor(0, 1);
    lcd.print(CREDITS_LINE2);
    lcd.setCursor(0, 2);
    lcd.print(CREDITS_LINE3);
}

// Liest Lichtsensor
int checkBrightness() {
  return analogRead(light_sens);
}

// Schaltet die RGB-LED nach Zeitdifferenz
void switchColor(uint8_t color) {
  if (color == 0) {
      digitalWrite(red, LOW);
      digitalWrite(green, LOW);
      digitalWrite(blue, LOW);
  } else if (color == 1) {
      digitalWrite(red, HIGH);
      digitalWrite(green, LOW);
      digitalWrite(blue, LOW);
  } else if (color == 2) {
      digitalWrite(red, HIGH);
      digitalWrite(green, HIGH);
      digitalWrite(blue, LOW);
  } else if (color == 3) {
      digitalWrite(red, HIGH);
      digitalWrite(green, HIGH);
      digitalWrite(blue, HIGH);
  }
}

// Ändert Farbe, wenn die Weckzeit bald erreicht wird
void checkColor() {
  if(hour == weckerStunden && minute <= weckerMinuten) {
    int diff = weckerMinuten - minute;
    time_to_wecker = diff;
    if (diff > 10) switchColor(0);
    else if (diff > 5) switchColor(1);
    else if (diff > 1) switchColor(2);
    else if (diff == 1) switchColor(3);
    else switchColor(0);
  }
}

// Spielt Ton auf dem Buzzer
void playSound(int hz) {
  if (loudspeakerOn) tone(buzzer, hz);
}

// Stoppt Ton
void stopSound() {
  noTone(buzzer);
}

// Regelt Licht basierend auf Menü/Zeit/Helligkeit
void checkLighting() {
  if (isMenu) {
    lcd.backlight();
    display.setBrightness(7);
    return;
  }
  if (!isTimeReceived) {
    lcd.noBacklight();
    display.setBrightness(0);
    return;
  }
  if (automatic_lightdimming) {
    int currentBrightness = checkBrightness();
    if (currentBrightness > 512) {
      lcd.backlight();
      display.setBrightness(7);
    } else {
      lcd.noBacklight();
      display.setBrightness(0);
    }
  } else {
    lcd.backlight();
    display.setBrightness(7);
  }
  int num = hour * 100 + minute;
  display.showNumberDecEx(num, 0b11100000, true);
}

// Liest Zeit von Serial, setzt Uhr, gibt "TIME_RECEIVED" zurück
void checkForNewTime() {
  if (Serial.available() <= 0) return;
  String incomingData = Serial.readStringUntil('\n');
  int firstCommaIndex  = incomingData.indexOf(',');
  int secondCommaIndex = incomingData.indexOf(',', firstCommaIndex + 1);
  if (firstCommaIndex == -1 || secondCommaIndex == -1) {
    Serial.println("TIME_RECEIVED");
    return;
  }
  String incomingTime     = incomingData.substring(0, firstCommaIndex);
  String incomingWeekday  = incomingData.substring(firstCommaIndex + 1, secondCommaIndex);
  String incomingMonthDay = incomingData.substring(secondCommaIndex + 1);
  int firstColonIndex  = incomingTime.indexOf(':');
  int secondColonIndex = incomingTime.indexOf(':', firstColonIndex + 1);

  if (firstColonIndex == -1) {
    hour = 0;
    minute = 0;
  } else {
    hour = incomingTime.substring(0, firstColonIndex).toInt();
    if (secondColonIndex == -1) {
      minute = incomingTime.substring(firstColonIndex + 1).toInt();
    } else {
      minute = incomingTime.substring(firstColonIndex + 1, secondColonIndex).toInt();
    }
  }
  day = incomingWeekday;
  int dashIndex = incomingMonthDay.indexOf('-');
  if (dashIndex != -1) {
    month = incomingMonthDay.substring(0, dashIndex).toInt();
    dayy  = incomingMonthDay.substring(dashIndex + 1).toInt();
  }
  Serial.println("TIME_RECEIVED");
  if(!isTimeReceived) {
    isTimeReceived = true;
  }
  if(!isMenu) {
    displayTime(hour, minute, day, automatic_lightdimming);
  } else {
    int num = hour * 100 + minute;
    if(num == 0) display.showNumberDecEx(0, 0b11100000, true);
    else display.showNumberDecEx(num, 0b11100000, true);
  }
}

// Checkt Weckerzeit und piept ggf.
void checkWakeUpTime() {
  if(hour == weckerStunden && minute == weckerMinuten && wokeCounter < wokeMax) {
    wokeCounter++;
    playSound(2000); delay(100); stopSound();
    playSound(4000); delay(100); stopSound();
  } else if (minute != weckerMinuten && wokeCounter == wokeMax) {
    wokeCounter = 0;
  }
}

// Liest Poti, scrollt im Menü oder ändert Weckerzeit
void handlePotiScroll() {
  int potiValue = analogRead(POTI_PIN);
  if (abs(potiValue - lastPotiValue) > HYSTERESE) {
    lastPotiValue = potiValue;
    uint8_t newIndex;
    if (isSettingHours) {
      weckerStunden = map(potiValue, 0, MAX_POTI_VALUE, 0, 23);
      updateWeckzeit();
    }
    else if (isSettingMinutes) {
      weckerMinuten = map(potiValue, 0, MAX_POTI_VALUE, 0, 59);
      if(weckerMinuten > 59) weckerMinuten = 59;
      updateWeckzeit();
    }
    else if (!isSubMenu) {
      newIndex = map(potiValue, 0, MAX_POTI_VALUE, 0, menuLength - 1);
      newIndex = constrain(newIndex, 0, menuLength - 1);
      if (newIndex != menuIndex) {
        menuIndex = newIndex;
        needsRedraw = true;
        displayMenu();
      }
    }
    else if (isShowingCredits) {
    }
    else {
      uint8_t subMenuLength = subMenuLengths[menuIndex];
      newIndex = map(potiValue, 0, MAX_POTI_VALUE, 0, subMenuLength - 1);
      newIndex = constrain(newIndex, 0, subMenuLength - 1);
      if (newIndex != subMenuIndex) {
        subMenuIndex = newIndex;
        needsRedraw = true;
        lastWeckzeitDrawn = false;
      }
    }
  }
}

// Liest Button, entprellt, schaltet Menü und Optionen
void handleButtonPress() {
  bool currentButtonState = digitalRead(BUTTON_PIN);
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY &&
      lastButtonState == LOW &&
      currentButtonState == HIGH)
  {
    if (isMenu) {
      if (isSettingHours) {
        isSettingHours = false;
        isSettingMinutes = true;
        needsRedraw = true;
      }
      else if (isSettingMinutes) {
        isSettingMinutes = false;
        isSubMenu = true;
        needsRedraw = true;
      }
      else if (!isSubMenu && menuIndex == 3) {
        isMenu = false;
        if(isTimeReceived) {
          checkLighting();
          displayTime(hour, minute, day, automatic_lightdimming);
        }
      }
      else if (!isSubMenu) {
        isSubMenu = true;
        subMenuIndex = 0;
        needsRedraw = true;
      }//Stundeneinstellung
      else if (menuIndex == 0 && subMenuIndex == 0) {
        isSettingHours = true;
        needsRedraw = true;
      }//Minuteneinstellung
      else if (menuIndex == 0 && subMenuIndex == 1) {
        isSettingMinutes = true;
        needsRedraw = true;
      } //Lichteinstellung
      else if (menuIndex == 1 && subMenuIndex == 0) {
        automatic_lightdimming = !automatic_lightdimming;
        displaySubMenu();
      }//Lautsprechereinstellung
      else if (menuIndex == 1 && subMenuIndex == 1) {
        loudspeakerOn = !loudspeakerOn;
        displaySubMenu();
      }//Creditsmenü
      else if (isShowingCredits) {
        isShowingCredits = false;
        displaySubMenu();
      }//Creditsmenü
      else if (menuIndex == 2 && subMenuIndex == 1) {
        isShowingCredits = true;
        displayCredits();
      }//Zurück
      else {
        isSubMenu = false;
        needsRedraw = true;
        displayMenu();
      }
    }
    else {
      isMenu = !isMenu;
      if(isMenu) {
        displayMenu();
      } else {
        if(isTimeReceived) {
          checkLighting();
          displayTime(hour, minute, day, automatic_lightdimming);
        }
      }
    }
  }
  lastButtonState = currentButtonState;
}



void setup() {
  pinMode(POTI_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(red, OUTPUT);
  pinMode(green, OUTPUT);
  pinMode(blue, OUTPUT);

  Serial.begin(9600);
  while (!Serial) {}

  lcd.init();
  lcd.noBacklight();
  display.clear();
  display.setBrightness(0);

  switchColor(1); delay(500);
  switchColor(2); delay(500);
  switchColor(3); delay(500);
  switchColor(0);

  Serial.println("Arduino is ready to receive time updates.");
}

void loop() {
  handleButtonPress();
  checkForNewTime();
  checkLighting();
  checkWakeUpTime();
  checkColor();

  if (isMenu) {
    handlePotiScroll();
    if (isSubMenu) {
      if (needsRedraw) {
        displaySubMenu();
        needsRedraw = false;
      }
      if (menuIndex == 0 && !lastWeckzeitDrawn) {
        updateWeckzeit();
        lastWeckzeitDrawn = true;
      }
    }
  }
}
