/******************************************************************************************************
  Programmcode zur Diplomarbeit Dichtemessgerät
  Diplomand:        Raphael Lindauer
  Erstelldatum:     16.08.2021
  Fertigungsdatum:  11.10.2021

  doCalibration()----------------------------------
  Dieser Funktionsaufruf wird durch das gleichzeitige Betätigen der Tare- und FunctionTaste aufgerufen.
  Die Funktion selbst dient zur Neukalibrierung der Wage indem einerseits die Waage ohne Gewicht und
  mit einem entsprechenden Kalibrationsgewicht ausgemessen wird um eine 2-Punkt kalibrierung zu ermöglichen.
  Der entsprechende Kalibrationsfaktor wird berechnet, auf dem LCD ausgegeben und im internen EEPROM
  abgespeichert. Somit steht der Kalibrationsfaktor auch nach einem Stromunterbruch und bei einem
  Neustart stets zur Verfügung.

  doTare()------------------------------------------
  In diesem Funktionsaufruf wird die Waage durch den Bibliotheksbefehl "LoadCell.tare()" genullt und die
  Bestätigung nach erfolgreichem ausführen auf dem LCD für den Anwender ausgegeben.

  showScreenview()----------------------------------
  In diesem Funktionsaufruf werden sämtliche Sensordaten entsprechend für den Anwender dargestellt. Dabei
  wird das Gewicht in g, das Volumen in ml und die Dichte in g/ml mit dem entsprechenden Sensorwert
  dargestellt. Durc die Betätigung der Function-Taste kann die Anzeige entsprechend verändert werden.

  getMeasurements()---------------------------------
  Durch diesen Funktionaufruf werden die Sensordaten der Wägezelle und des Abstandssensors ausgelesen und verarbeitet.
  Aus dem Wert der Höhensensorik wird durch eine Berechnung das Volumen errechnet.

*/


// Bibliotheken einbinden --------------------------------------------------------
#include <HX711_ADC.h>                                                                        // A/D Wandlung des Gewichtsensors und Ansteuerung der Wägezelle TAL221
#include <Wire.h>                                                                             // Generelle I2C Kommunikation 
#include <SerLCD.h>                                                                           // Ansteuerung des LCD's über I2C
#include <EEPROM.h>                                                                           // Ansteuerung des internen EEPROM
#include <Bounce2.h>                                                                          // Tasterentprellung der Tare und Function Taste



// Hardware Konfiguration festlegen-------------------------------------------------
// Eingabetasten
#define Button_Tare 7
#define Button_Function 8
Bounce2::Button ButtonTare = Bounce2::Button();                                               // Funktion zur entprellung der Taster
Bounce2::Button ButtonFunction = Bounce2::Button();


// Konfiguration Pins des Ultraschallsensors
const int TRIG_PIN = 12;
const int ECHO_PIN = 13;


// Konfiguration Waage
const int HX711_dout = 3;                                                                      // Data-Pin des HX711 A/D-Wandler
const int HX711_sck = 2;                                                                       // Clock-Pin des HX711 A/D-Wandler
HX711_ADC LoadCell(HX711_dout, HX711_sck);                                                     // Übergabe der beiden digitalen Pins für die Nutzung der HX711-Bibliothek

// Konfiguration LCD
SerLCD lcd;                                                                                    // Initalisiere LCD mit Standart Adresse 0x72


// Variabeldefinitionen--------------------------------------------------------------
// Variabeldefinitionen Waage
const int calVal_eepromAdress = 0;                                                             // Speicheradresse des EEPROM für den Kalibrationswert der Waage
unsigned long stabilizingtime = 2000;                                                          // Stabilisierungszeit nach Neustart um Genauigkeit zu erhöhen
boolean resumeCalib = false;
const float calibrationWeight = 500.00;
float calibrationValue = 1.0;
float weight_g = 0;

// Variabeldefinitionen Volumen
float refdistance_mm = 165.00;
float distance_mm = 0;
int duration;
int t = 0;

// Variabeldefinition Volumen- und Dichteberechnung
float density_gml = 0;
float volume_ml = 0;
float volume_average_ml = 0;
float B = 0;
int immernsche_scale = 0;

// Allgemeine Variabeln
int screenview = 0;


void setup() {

  // Tare und Function Taster Hardwarekonfiguration mit Entprellungsfunktion
  ButtonTare.attach(Button_Tare);
  ButtonFunction.attach(Button_Function);
  ButtonTare.interval(50);                                                                     // Debouncezeit
  ButtonFunction.interval(50);
  ButtonTare.setPressedState(HIGH);
  ButtonFunction.setPressedState(HIGH);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);


  // Beginn I2C Verbinfung mit dem LCD, Kontrast und Backlight einstellen
  Wire.begin();                                                                                // Meldet den I2C Bus an
  lcd.begin(Wire);                                                                             // Initialisiere die LCD Anzeige für die I2C Kommunikation
  lcd.setBacklight(255, 255, 255);
  lcd.setContrast(5);
  lcd.clear();

  // Startup Prozess
  lcd.setCursor(0, 0);
  lcd.print("Starting...");
  LoadCell.begin();                                                                            // Starte Verbindung mit dem HX711

  EEPROM.begin(); // EEPROM Kommunikation starten um den Kalibrationswert auszulesen
  EEPROM.get(calVal_eepromAdress, calibrationValue);
  LoadCell.start(stabilizingtime);                                                             // Wägezelle kann sich für 2000ms stabilisieren und wird genullt


  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {                      // Kontrolle ob Kommunikation mit dem HX711 hergestellt werden kann
    lcd.clear();
    lcd.print("Timeout");                                                                      // Fehlerausgabe auf das LCD bei Verbindungsproblemen mit dem HX711
    lcd.setCursor(0, 1);
    lcd.print("check HX711");
    while (1);
  }
  else {
    LoadCell.setCalFactor(calibrationValue);                                                   // Kalibrationswert aus dem EEPROM wird übergeben
    lcd.clear();
    lcd.print("Startup complete");
    delay(1000);
    lcd.clear();
  }

}

void loop() {

  // Abfrage der Eingabetasten -------------------------------------------------------------------
  ButtonTare.update();
  if (ButtonTare.pressed()) {
    ButtonFunction.update();
    if (ButtonFunction.pressed()) {
      doCalibration();                                                                          // Starte den Kalibrationsprozess
    }
    else {
      doTare();                                                                                 // Funktion um Waage zu Nullen
    }
  }

  ButtonFunction.update();
  if (ButtonFunction.pressed()) {
    lcd.clear();
    if (screenview < 1) {
      screenview = screenview + 1;
    }
    else {
      screenview = 0;
    }
  }

  getMeasurements();          // Sensordaten abfragen
  doCalculations();           // Berechnungen mit den Sensordaten durchführen
  getImmernschScale();        // Übersetzung der Dichte in die Immernsche Skala
  showScreenview();           // Sensordaten auf dem Display ausgeben
}

// Ausgabe der Sensordaten auf das LCD ------------------------------------------------------------
void showScreenview() {
  switch (screenview) {

    case 0:
      lcd.setCursor(0, 0);
      lcd.print("Gewicht");
      lcd.setCursor(9, 0);
      lcd.print(weight_g, 1);
      lcd.setCursor(14, 0);
      lcd.print("g");
      lcd.setCursor(0, 1);
      lcd.print("Volumen");
      lcd.setCursor(9, 1);
      lcd.print(volume_average_ml, 0);
      lcd.setCursor(12, 1);
      lcd.print("ml");
      break;

    case 1:
      lcd.setCursor(0, 0);
      lcd.print("Dichte");
      lcd.setCursor(0, 1);
      lcd.print(density_gml, 3);
      lcd.setCursor(6, 1);
      lcd.print("g/ml");
      lcd.setCursor(8, 0);
      lcd.print("Im.Skala");
      if (immernsche_scale == 0) {
        lcd.setCursor(14, 1);
        lcd.print(" ");
        lcd.setCursor(15, 1);
        lcd.print(immernsche_scale);
      }
      else {
        lcd.setCursor(14, 1);
        lcd.print(immernsche_scale);
      }

      break;
  }
}

// Tarefunktion zur Nullung der Waage -------------------------------------------------------------
void doTare() {
  lcd.clear();
  lcd.print("Taring...");
  LoadCell.tare();                                                                                           // Nullung der Waage
  lcd.clear();
  lcd.print("Tare complete");
  delay(1000);
  lcd.clear();
}

// Messdaten von den Sensoren ---------------------------------------------------------------------
void getMeasurements() {

  if (LoadCell.update()) {
    weight_g = LoadCell.getData();                                                                            // Auslesen der Wägezelle und speichern des aktuellen Gewichtwertes
  }

  if (millis() > t + 100) {                                                                                  // Auslesen des Distanzsensors und berechnung der Entfernung jede Sekunde
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(50);
    digitalWrite(TRIG_PIN, LOW);
    duration = pulseIn(ECHO_PIN, HIGH);
    distance_mm = (duration / 5.78);                                                                          // Faktor 5.77 entspricht Schallgeschwindigkeit bei Raumtemperatur
    t = millis();
  }
}


// Berechnung des Füllvolumens und der Dichte im Messbecher ----------------------------------------
void doCalculations() {
  // Volumenberechung mit gleitender Mittelwertbildung (Tiefpassfilter)
  B = 7.3 + (8.3 - 7.3) * (((refdistance_mm - distance_mm) / 10) / 10.4);                                   // Berechnung des Füllvolumens im Becher aus den Daten des Höhensensors
  volume_ml = ((((refdistance_mm - distance_mm) * PI) / 12) * (sq(B) + B * 7.3 + sq(7.3))) / 10;

  if (volume_ml < 100) {                                                                                      // Sofern das Volumen kleiner 100ml ist, soll 0 ausgegeben werden
    volume_ml = 0;
  }

  volume_average_ml = ((volume_average_ml * 9) + volume_ml) / (9 + 1.0);                                    // Tiefpassfilter Faktor 9 = Mittelwert besteht zu 90% aus altem und 10% aus neuem Wert

  if (weight_g < 0 || volume_average_ml < 100) {                                                              // Sofern das Gewicht negativ oder das berechnete Volumen unter 100 liegt soll für die Dichte 0 ausgegeben werden
    density_gml = 0;
  }
  else
  {
    density_gml = weight_g / volume_average_ml;                                                             // Berechnung der Dichte im Messbecher
  }
}


void getImmernschScale() {

  if (density_gml < 0.21 && density_gml > 0.1) {
    immernsche_scale = 55;
  }
  else if (density_gml <= 0.24 && density_gml > 0.2) {
    immernsche_scale = 45;
  }
  else if (density_gml > 0.24 && density_gml < 0.30) {
    immernsche_scale = 35;
  }
  else if (density_gml > 0.3 && density_gml < 0.36) {
    immernsche_scale = 25;
  }
  else if (density_gml > 0.36 && density_gml < 0.5) {
    immernsche_scale = 15;
  }
  else {
    immernsche_scale = 0;
  }
}



// Funktion zur Kalibrierung der Waage ---------------------------------------------------------------
void doCalibration() {

  lcd.clear();
  lcd.print("Start");
  lcd.setCursor(0, 1);
  lcd.print("Calibration");
  delay(1000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Remove any load");
  lcd.setCursor(0, 1);
  lcd.print("and Press Tara");

  resumeCalib = false;
  while (resumeCalib == false) {                                                                            // Nullung der Waage
    LoadCell.update();
    ButtonTare.update();
    if (ButtonTare.pressed()) {
      LoadCell.tare();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Tare complete");
      resumeCalib = true;
    }
  }
  lcd.clear();
  lcd.print("Place 500g on");
  lcd.setCursor(0, 1);
  lcd.print("scale&press tare");
  resumeCalib = false;
  while (resumeCalib == false) {                                                                            // Generierung des Kalibrationsfaktors mit 500g Referenzgewicht
    LoadCell.update();
    ButtonTare.update();
    if (ButtonTare.pressed()) {
      LoadCell.refreshDataSet();
      calibrationValue = LoadCell.getNewCalibration(calibrationWeight); // Neuen Kalibrationswert laden
      lcd.clear();
      lcd.print("Cal. Wert Neu:");
      lcd.setCursor(0, 1);
      lcd.print(calibrationValue);

      delay(1000);
      lcd.clear();
      lcd.print("Cal. Faktor wird");
      lcd.setCursor(0, 1);
      lcd.print("gespeichert");

      resumeCalib = true;
    }
  }


#if defined(ESP8266)|| defined(ESP32)
  EEPROM.begin(512);
#endif
  EEPROM.put(calVal_eepromAdress, calibrationValue);                                                          // Speicherung des Kalibrationsfaktors auf das EEPROM
#if defined(ESP8266)|| defined(ESP32)
  EEPROM.commit();
#endif
  EEPROM.get(calVal_eepromAdress, calibrationValue);
  lcd.clear();
  lcd.print("Value ");
  lcd.print(calibrationValue);
  lcd.setCursor(0, 1);
  lcd.print("EEPROM adr:");
  lcd.print(calVal_eepromAdress);
  delay(1000);

  lcd.clear();
  lcd.print("Calib. beendet");
  delay(500);
  lcd.clear();
}
