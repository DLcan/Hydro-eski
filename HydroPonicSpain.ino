// HidroponicoCole_v5.8 NO CLAVES
//
// bluetooht RX3-TX3
// DALLAS pin 3
// DHT pin 2
// Bomba pin 6
// Luz pin 7
// Shield ESP 8266 con comandos AT
// Utiliza libreria TimeLib para la fecha y hora
// Envia datos cada 15 minutos
// LCD con ALARMAS
// Invierte salida Bomba y Luz 0= activo, 1= inactivo
// -----------------------------------------------------------

#define DEBUG 0                                // change value to 1 to enable debuging using serial monitor  
String network = "SSID NAME";                  // your access point SSID
String password = "PASSWORD";                  // your wifi Access Point password
#define IP "184.106.153.149"                   // IP address of thingspeak.com  184.106.153.149
String GET = "GET /update?key=CHANNEL_KEY";    // replace with your channel key


#include "OpenGarden.h"
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal.h>
#include <TimeLib.h>

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 51, en = 53, d4 = 39, d5 = 37, d6 = 35, d7 = 33;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Establece valores inicio de fecha y hora
int hora = 9;
int minuto = 0;
int segundo = 0;
int dia = 1;
int mes = 1;
int ano = 17;

bool nuevoSegundo;
int viejoSegundo = 0;

bool nuevoMinuto;
int viejoMinuto = 0;

bool nuevaHora;
int viejaHora = 0;


// variables telegrama recibido de bluethooh
// cabecera, cuerpo1, cuerpo2, cuerpo3, fin
int cabecera = 0;
int cuerpo1 = 0;
int cuerpo2 = 0;
int cuerpo3 = 0;
int fin = 0;

// Sensores PH y EC
#define calibration_point_4 2246  //Write here your measured value in mV of pH 4
#define calibration_point_7 2080  //Write here your measured value in mV of pH 7
#define calibration_point_10 1894 //Write here your measured value in mV of pH 10

#define point_1_cond 40000   // Write here your EC calibration value of the solution 1 in µS/cm
#define point_1_cal 40       // Write here your EC value measured in resistance with solution 1
#define point_2_cond 10500   // Write here your EC calibration value of the solution 2 in µS/cm
#define point_2_cal 120      // Write here your EC value measured in resistance with solution 2

/*   SENSOR DHT22 (AIRE)  */
#define DHTPIN 2
#define DHTTYPE DHT22

float TemperaturaAire;
float HumedadAire;
DHT dht(DHTPIN, DHTTYPE);


/*   SENSOR Temperatura DALLAS  (TemperaturaAgua) */
#define ONE_WIRE_BUS 3
OneWire oneWireBus (ONE_WIRE_BUS);
DallasTemperature sensors (&oneWireBus);
float TemperaturaAgua;

/*   BOMBA y LUZ   */
#define PinBombaAgua 6  // Bomba en pin 6
#define PinLuz 7        // Luces en pin 7
#define Amanece 8      // Hora de encendido Luz  
#define Anochece 20    // Hora apagado Luz
#define MinutosBomba 20    // Minutos funcionando bomba
bool BombaAgua = 0;   // 0=parada , 1= marcha
bool Luz = 0;         // 0= apagada , 1= encendida

// VALORES DE ALARMAS
#define PhAlto 10          // Valor alto alarma Ph
#define PhBajo 5           // Valor bajo alarma Ph
#define EcAlto 3000        // Valor alto alarma Ec
#define EcBajo 900         // Valor bajo alarma Ec
// Valor EcMuyBajo activa "Falta de agua". NO PERMITE FUNCIONAMIENTO BOMBA
#define EcMuyBajo 200
#define TempAguaAlto 40    // Valor alto alama Temp Agua
#define TempAguaBajo 5     // Valor bajo alama Temp Agua
int AlarmaPH;         // alarma Ph
int AlarmaTempAgua;   // alarma Temp
int AlarmaEC;         // alarma Ec

float pH;
float EC;


void setup() {
  lcd.begin(16, 2);                 // Inicia LCD 16 caracteres, 2 filas
  // Mensaje de arranque en LCD
  borrarLCD();
  lcd.setCursor(0, 0);              // posiciona cursor linea 0, columna 0
  lcd.print("INICIANDO");
  lcd.setCursor(0, 1);
  lcd.print("ESPERE .....");

  setupEsp8266();                    // inicia conexión WiFi

  pinMode(PinBombaAgua, OUTPUT);
  pinMode(PinLuz, OUTPUT);

  Serial3.begin(9600);
  Serial.begin(115200);

  // establece fecha y hora al arrancar
  setTime(hora, minuto, segundo, dia, mes, ano);

  // Start up the libraries
  sensors.begin(); // DALLAS
  dht.begin(); // DHT

  OpenGarden.initSensors(); //Initialize sensors power
  OpenGarden.sensorPowerON();//Turn On the sensors
  OpenGarden.calibratepH(calibration_point_4, calibration_point_7, calibration_point_10);
  OpenGarden.calibrateEC(point_1_cond, point_1_cal, point_2_cond, point_2_cal);
  delay(500);
}

void loop() {

  // Read DALLAS
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  sensors.requestTemperatures(); // Send the command to get temperatures
  TemperaturaAgua = (sensors.getTempCByIndex(0)); // Why "byIndex"?
  // You can have more than one IC on the same bus.
  // 0 refers to the first IC on the wire

  // Lee DHT 22
  HumedadAire = dht.readHumidity();
  TemperaturaAire = dht.readTemperature();


  //Read the pH sensor
  int mvpH = OpenGarden.readpH(); //Value in mV of pH
  pH = OpenGarden.pHConversion(mvpH); //Calculate pH value
  if ( pH < 0 || pH > 14) {
    pH = 0 ;
  }

  //Read the conductivity sensor in µS/cm
  float resistanceEC = OpenGarden.readResistanceEC(); //EC Value in resistance
  EC = OpenGarden.ECConversion(resistanceEC); //EC Value in µS/cm

  // Alarmas datos Agua
  AlarmaPH = 0;  // Resetea el valor de la alarma Ph
  if ( pH > PhAlto ) {
    AlarmaPH = 2 ;
  }
  if ( pH < PhBajo ) {
    AlarmaPH = 1 ;
  }

  AlarmaTempAgua = 0;  // Resetea el valor de la alarma Temp Agua
  if ( TemperaturaAgua > TempAguaAlto ) {
    AlarmaTempAgua = 2 ;
  }
  if ( TemperaturaAgua < TempAguaBajo ) {
    AlarmaTempAgua = 1 ;
  }

  AlarmaEC = 0;  // Resetea el valor de la alarma EC
  if ( EC > EcAlto ) {
    AlarmaEC = 2 ;
  }
  if ( EC < EcBajo ) {
    AlarmaEC = 1 ;
  }
  if ( EC < EcMuyBajo ) {
    AlarmaEC = 3 ;
  }

  // Construye y envia a ESP 8266
  if (viejoMinuto != minute()) {
    nuevoMinuto = true;
    viejoMinuto = minute();
  } else {
    nuevoMinuto = false;
  }

  if (minute() % 15 == 0 && nuevoMinuto) {     //  5= cada 5 minutos,  15= cada 15 minutos
    updateTemp(String(pH) , String(EC), String(TemperaturaAgua), String(TemperaturaAire), String(HumedadAire));
  }

  // comprobar recepción datos desde bluetooth
  if (Serial3.available () > 10) {
    cabecera = Serial3.parseInt ();
    cuerpo1 = Serial3.parseInt ();
    cuerpo2 = Serial3.parseInt ();
    cuerpo3 = Serial3.parseInt ();
    fin = Serial3.parseInt ();
    String basura = Serial3.readString(); // vacía el buffer de lectura
  }

  if (cabecera == fin && cabecera == 20) {   // si cabecera=fin=20 actualiza hora
    setTime(cuerpo1, cuerpo2, cuerpo3, dia, mes, ano);
    cabecera = 0; // borra cabecera y fin para no repetir
    fin = 0;
  }

  // Envía datos por Bluetooth
  Serial3.print("<");
  Serial3.print(pH);
  Serial3.print(", ");
  Serial3.print(EC);
  Serial3.print(", ");
  Serial3.print(TemperaturaAgua);
  Serial3.print(", ");
  Serial3.print(HumedadAire);
  Serial3.print(", ");
  Serial3.print(TemperaturaAire);
  Serial3.print(", ");
  Serial3.print(hour());   // envia hora actual
  Serial3.print(", ");
  Serial3.print(minute()); // envia minuto actual
  Serial3.print(", ");
  Serial3.print(second()); // envia segundo actual
  Serial3.print(", ");
  Serial3.print(BombaAgua); //envia estado BombaAgua
  Serial3.print(", ");
  Serial3.print(Luz); // envia estado Luz
  Serial3.print(">");

  // control bomba de agua minutos cada hora
  if (minute() < MinutosBomba  && EC > EcMuyBajo) {    // EC muy bajo implica riego de falta de agua
    digitalWrite (PinBombaAgua, LOW);  // LOW = Bomba on
    BombaAgua = 1;
  }
  else {
    digitalWrite (PinBombaAgua, HIGH);  // HIGH = Bomba off
    BombaAgua = 0;
  }

  // control luz encendida de Amanece a Anochece
  if (hour() > Amanece && hour() < Anochece) {
    digitalWrite (PinLuz, LOW);          // LOW = Luz on
    Luz = 1;
  }
  else {
    digitalWrite (PinLuz, HIGH);          // HIGH = Luz off
    Luz = 0;
  }

  // refresca LCD cada segundo
  if (viejoSegundo != second()) {
    nuevoSegundo = true;
    viejoSegundo = second();
  } else {
    nuevoSegundo = false;
  }

  if (nuevoSegundo == true) {
    visualiza ();
  }

}

//-------------------------------------------------------------------
// Following function setup the esp8266, put it in station mode and
// connect to wifi access point.
//------------------------------------------------------------------
void setupEsp8266()
{
  if (DEBUG) {
    //Serial3.println("Reseting esp8266");
  }
  Serial.flush();
  Serial.println(F("AT+RST"));
  delay(7000);


  if (Serial.find("OK"))
  {
    if (DEBUG) {
      Serial3.println("Found OK");
      Serial3.println("Changing espmode");
    }
    Serial.flush();
    changingMode();
    delay(5000);
    Serial.flush();
    connectToWiFi();
  }
  else
  {
    if (DEBUG) {
      Serial3.println("OK not found");
    }
  }
}

//-------------------------------------------------------------------
// Following function sets esp8266 to station mode
//-------------------------------------------------------------------
bool changingMode()
{
  Serial.println(F("AT+CWMODE=1"));
  if (Serial.find("OK"))
  {
    if (DEBUG) {
      Serial3.println("Mode changed");
    }
    return true;
  }
  else if (Serial.find("NO CHANGE")) {
    if (DEBUG) {
      Serial3.println("Already in mode 1");
    }
    return true;
  }
  else
  {
    if (DEBUG) {
      Serial3.println("Error while changing mode");
    }
    return false;
  }
}

//-------------------------------------------------------------------
// Following function connects esp8266 to wifi access point
//-------------------------------------------------------------------
bool connectToWiFi()
{
  if (DEBUG) {
    Serial3.println("inside connectToWiFi");
  }
  String cmd = F("AT+CWJAP=\"");
  cmd += network;
  cmd += F("\",\"");
  cmd += password;
  cmd += F("\"");
  Serial.println(cmd);
  delay(15000);

  if (Serial.find("OK"))
  {
    if (DEBUG) {
      Serial3.println("Connected to Access Point");
    }
    return true;
  }
  else
  {
    if (DEBUG) {
      Serial3.println("Could not connect to Access Point");
    }
    return false;
  }
}

//-------------------------------------------------------------------
// Following function sends sensor data to thingspeak.com
//-------------------------------------------------------------------
void updateTemp(String valor1, String valor2, String valor3, String valor4, String valor5)
{
  String cmd = "AT+CIPSTART=\"TCP\",\"";
  cmd += IP;
  cmd += "\",80";
  Serial.println(cmd);
  if (DEBUG) {
    Serial3.println (cmd);
  }

  delay(5000);
  if (Serial.find("Error")) {
    if (DEBUG) {
      Serial3.println("ERROR while SENDING");
    }
    return;
  }
  cmd = GET + "&field1=" + valor1 + "&field2=" + valor2 + "&field3=" + valor3 + "&field4=" + valor4 + "&field5=" + valor5 + "\r\n";
  if (DEBUG) {
    Serial3.println (valor1);
    Serial3.println (valor2);
    Serial3.println (valor3);
    Serial3.println (valor4);
    Serial3.println (valor5);
    Serial3.println (cmd);
  }


  Serial.print("AT+CIPSEND=");
  Serial.println(cmd.length());
  delay(15000);
  if (Serial.find(">"))
  {
    Serial.print(cmd);
    if (DEBUG) {
      Serial3.println("Data sent");
    }
  } else
  {
    Serial.println("AT+CIPCLOSE");
    if (DEBUG) {
      Serial3.println("Connection closed");
    }
  }
}

// -------------------------------------
//  Muetra datos LCD
// -------------------------------------
void visualiza() {

  // visualiza fecha y hora
  if (second() % 30 >= 0 && second() % 30 < 7) {
    borrarLCD();
    lcd.setCursor(0, 0); // posiciona cursor linea 0, columna 0
    lcd.print("HORA ACTUAL");


    lcd.setCursor(0, 1);
    lcd.print(format(hour()));
    lcd.print(":");
    lcd.print(format(minute()));
    lcd.print(":");
    lcd.print(format(second()));
  }

  // visualiza datos AGUA
  if (second() % 30 >= 7 && second() % 30 < 14) {
    borrarLCD();
    lcd.setCursor(0, 0); // posiciona cursor linea 0, columna 0
    lcd.print("AGUA: ");
    lcd.print((int)EC);
    lcd.print(" uS/cm");

    lcd.setCursor(0, 1);
    lcd.print("pH=");
    lcd.print(pH);
    lcd.print("; ");
    lcd.print(TemperaturaAgua);
    lcd.print(" C");

  }

  // visualiza datos AIRE
  if (second() % 30 >= 14 && second() % 30 < 21) {
    borrarLCD();
    lcd.setCursor(0, 0); // posiciona cursor linea 0, columna 0
    lcd.print("    AIRE ");

    lcd.setCursor(0, 1); // posiciona cursor linea 0, columna
    lcd.print((int)TemperaturaAire);
    lcd.print(" C  ;  ");
    lcd.print((int)HumedadAire);
    lcd.print("%");

  }

  // visualiza ALARMAS
  if (second() % 30 >= 21 && second() % 30 < 30) {
    borrarLCD();
    lcd.setCursor(0, 0); // posiciona cursor linea 0, columna 0
    lcd.print("   ALARMAS ");

    lcd.setCursor(0, 1); // posiciona cursor linea 0, columna
    if (AlarmaPH == 0 && AlarmaTempAgua == 0 && AlarmaEC == 0) {    // verifica si hay alarmas
      lcd.print("NO HAY ALARMAS");
    }
    else {
      if (AlarmaPH > 0) {
        lcd.print("pH;");
      }
      if (AlarmaTempAgua > 0) {
        lcd.print("Temp Agua;");
      }
      if (AlarmaEC > 0 && AlarmaEC < 3 ) {
        lcd.print("EC");
      }
      if (AlarmaEC == 3 ) {
        lcd.print("No Agua");
      }
    }
  }
}

void borrarLCD() {
  lcd.setCursor(0, 0);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print("                ");
}

String format(int info) {
  String infoEditada;
  if (info < 10) {
    infoEditada += 0;
  }
  infoEditada += info;

  return infoEditada;
}

