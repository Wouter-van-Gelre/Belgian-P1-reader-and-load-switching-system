/*
Belgian smart meter reader and load switching system.

Reads the smart meter P1-port and switches consumers on and off based on total power consumption and injection.
Belgian smart meter communication is based on the Dutch DSMR 5.0 protocol, but there are (undocumented) differences.
Tested for the Sagemcom Siconia T211 meter. 

Hardware:
- Arduino Mega 2560
- IC 7404 NOT gate inverter
- TRU COMPONENTS TC-9927216 relay module, 4x 10A 250V relays (for switching the load-switching relays)
- 25 Ampère 250 Volt relays (for load-switching)

P1-port communication:
- serial port
- 5 Volt
- 115200bps
- 8N1: 8 bits, 1 stopbit
- inverted logic

P1-port pinout:
pin 1 +5V (for peripheral device)
pin 2 Data_Request: Arduino pin 8
pin 3 Data_GND: Arduino GND
pin 4 NC
pin 5 Data (open collector): IC 7404 pin 1, via 4.7kOhm to Arduino +5V
pin 6 Power_GND (for peripheral device)

IC 7404 pinout:
pin 1: P1 pin 5 (Data)
pin 2: Arduino 19RX1
pin 7: Arduino GND
pin 14: Arduino +5V

Arduino Mega 2560 pinout:
outputs:
pin +5V: IC 7404 pin 14, P1 pin 2, relay module VCC
pin GND: IC 7404 pin 7, P1 pin 3, pushbutton, relay module GND
pin 2: relay 1
pin 3: relay 2
pin 4: relay 3
pin 5: relay 4
pin 8: P1 pin 2 (Data_Request)
pin 13: internal LED
inputs:
pin 19RX1: IC 7404 pin 2
pin 9: pushbutton


Created 04 12 2023
By Wouter van Geldre
Modified 11 12 2023
By Wouter van Geldre
*/

#define BUFSIZE 1050 // vermoedelijke maximale regellengte Belgisch telegram, de 0.0:98.1.0 code, mogelijks de kwartierpieken (geen documentatie), bevat tot 1024 karakters.
char buffer[BUFSIZE]; // Buffer voor seriele data om \n te vinden.
int bufpos = 0; // teller voor ^^

#define DATAREQUESTPIN 8 // P1 serieële poort data_request pin
#define BOILERPIN 2 // relais pin
#define FIETSPIN 3  // relais pin
#define INDICATORPIN 5 // relais pin
#define BOILERBUTTONPIN 9 // drukknop boiler manueel aan
#define LEDPIN 13 // interne led pin

int minuten = 0; // lokale tijd in minuten sedert middernacht
int solarMinuten = 0; // zonnetijd in minuten
int PowerAfname = 0;	// huidig afgegenomen vermogen
int PowerInjectie = 0; // huidig geïnjecteerd vermogen
int boilerButtonState = 1; // staat van drukknop boiler manueel
unsigned long lastRead, lastEval; // milliseconds laatste telegram gelezen, laatste relais instelling
unsigned long boilerStartMillis, boilerManStartMillis, fietsStartMillis; // laatste inschakeling boiler, manuele inschakeling (en herinschakeling na verbruikspiek) van boiler, inschakeling fiets
int vijfMinuInjectie[300]; // laatste 5 minuten injectiewaarden
int vijfI = 0; // teller voor ^^
long vijfMinuGem = 0; // gemiddelde injectie over 5 minuten


void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  pinMode(DATAREQUESTPIN, OUTPUT);
  pinMode(BOILERPIN, OUTPUT);
  pinMode(FIETSPIN, OUTPUT);
  pinMode(INDICATORPIN, OUTPUT);
  pinMode(BOILERBUTTONPIN, INPUT_PULLUP);
  pinMode(LEDPIN, OUTPUT);

  for (int i=0; i<300; i++){ // array leeg maken
        vijfMinuInjectie[i] = 0;
  }
}

void loop(){
  readP1(); // P1 telegram lezen

  if (lastEval < lastRead){ // code enkel uitvoeren nadat volledig telegram gelezen is (om overflow Serial1 poortbuffer te voorkomen) en slechts 1 maal per ontvangen telegram
    setRelays();
  }
}

void readP1(){ // P1 telegram lezen, doe geen zware bewerkingen tijdens het lezen van het telegram, 115200bps is snel voor een Arduino Mega 2560

  char uur[3]; // uur uit P1 telegram
  char minu[3]; // min uit P1 telegram
  char dstcode[2]; // W = wintertijd; S = zomertijd
  long tl =0; // waarde kW uit telegramregel
  long tld =0; // waarde W uit telegramregel
  
  digitalWrite(DATAREQUESTPIN, HIGH);

  while (Serial1.available()) {
    char inchar = Serial1.read(); // lees 1 character
    //Serial.print(inchar); // debug, Serial1 poortbuffer overflowt wanneer je dit aanzet in combinatie met de rest van de code
 
    buffer[bufpos] = inchar&127; // vul buffer
    bufpos++;
 
    if (inchar == '!') { // einde telegram bereikt
      lastRead = millis();
    }  

    if (inchar == '\n') { // einde regel bereikt
      //Serial.print(buffer[0]); // debug, enkel het eerste karakter wordt getoond anders overflow Serial1 poortbuffer

      if (sscanf(buffer,"0-0:1.0.0(%*6c%2c%2c%*2c%c" , &uur, &minu, &dstcode) == 3 ) { // huidige lokale tijd
        String(struur) = uur;
        String(strmin) = minu;
        minuten = strmin.toInt() + struur.toInt() * 60;
        Serial.print("Minuten: ");
        Serial.println(minuten);

        // zonnetijd, waarden aan te passen aan lengtegraad
        if (dstcode == "S"){ // zomertijd
          solarMinuten = minuten - 107;
        }  
        else{ // wintertijd
          solarMinuten = minuten - 47;
        }
        
        Serial.print("Zonnetijd: ");
        Serial.println(solarMinuten);
      }    
      
      if (sscanf(buffer,"1-0:1.7.0(%ld.%ld" , &tl , &tld) == 2 ){ // huidige afname
        PowerAfname = tl * 1000 + tld;
        Serial.print("Afname: ");
        Serial.print(PowerAfname);
        Serial.println(" W");
      }

      if (sscanf(buffer,"1-0:2.7.0(%ld.%ld" , &tl , &tld) == 2 ){ // huidige injectie
        PowerInjectie = tl *1000 + tld;
        Serial.print("Injectie: ");
        Serial.print(PowerInjectie);
        Serial.println(" W");
      }            

      // Maak de buffer weer leeg (hele array)
      for (int i=0; i<BUFSIZE; i++){
        buffer[i] = 0;
      }
      
      bufpos = 0;
    }
  }
  digitalWrite(DATAREQUESTPIN, LOW);
}


void setRelays(){ // relais schakelen
  int Power = 0; // negatief bij injectie, positief bij afname

  // PV-installatie van 1260 WattPiek, basisverbruik is tussen 180 - 300 Watt

  // keukenboiler is 2200 Watt
  const int boilerAltijdAanWatt = 500; // injectie waarbij boiler steeds spanning krijgt
  const int boilerInschakelWatt = 400; // maximale afname waarbij boiler automatisch ingeschakeld mag worden
  const int boilerUitschakelWatt = 2800; // afname waarbij boiler uitschakelt
  const int boilerSolarStartMin = 705; // klokgestuurd vanaf ... zonnetijd
  const int boilerStopMin = 1260; // klokgestuurd tot 21u
  const unsigned long boilerManAanDuur = 1200000; // duur manuele inschakeling in milliseconden
  const unsigned long boilerHerinschakelWacht = 120000; // duur wachttijd herinschakeling na verbruikspiek
  bool boilerAan = 0; // resultaat evaluatie voorwaarden boiler

  // fietsoplader is ongeveer 180 Watt
  const int fietsAltijdAanWatt = 185; // injectie waarbij fietsoplader steeds spanning krijgt
  const int fietsInschakelWatt = 400; // maximale afname waarbij fiets automatisch ingeschakeld mag worden
  const int fietsUitschakelWatt = 2200; // afname waarbij fietsoplader uitschakelt
  const int fietsSolarStartMin = 645; // klokgestuurd vanaf ... zonnetijd
  const int fietsStopMin = 1005; // klokgestuurd tot 16u45
  bool fietsAan = 0; // resultaat evaluatie voorwaarden fietsoplader

  lastEval = millis();

  Power = PowerAfname - PowerInjectie;


  // gemiddelde injectie over 5 minuten berekenen
  if (vijfI < 299){
    vijfMinuInjectie[vijfI] = PowerInjectie;
    vijfI++;
  }
  else{
    vijfMinuInjectie[299] = PowerInjectie;
    vijfI = 0;
  }  

  for ( int i=0; i<300; i++){
    vijfMinuGem += vijfMinuInjectie[i];
  }

  vijfMinuGem = vijfMinuGem / 300;
  Serial.print("Gemiddelde afname 5 min: ");
  Serial.println(vijfMinuGem);


  // injectie/afname indicator

  if(PowerInjectie > 0){
    digitalWrite(INDICATORPIN, HIGH);
    digitalWrite(LEDPIN, LOW);
  }
  else{
    digitalWrite(INDICATORPIN, LOW);
    digitalWrite(LEDPIN, HIGH);
  }

  // boiler aansturing

  if (PowerInjectie > boilerAltijdAanWatt){ // voldoende injectie
    boilerAan = true;
  }

  if (solarMinuten > boilerSolarStartMin && minuten < boilerStopMin && PowerAfname < boilerInschakelWatt){ // klokgestuurd en lage afname
    boilerAan = true;
  }

  if (minuten > boilerStopMin){ // klokgestuurd uit
    boilerAan = false;
  }

  if (digitalRead(BOILERBUTTONPIN) == LOW){ // manueel inschakelen van de boiler voor bepaalde tijd, low bij gesloten, high bij open contact
    boilerManStartMillis = millis();
  }

  if (boilerManStartMillis >= millis() - boilerManAanDuur && boilerManStartMillis < millis()){ // duurtijd manuele inschakeling loopt
    boilerAan = true;
  }

  if (PowerAfname > boilerUitschakelWatt){ // afname te hoogherinschakelwachttijd loopt nog
    boilerAan = false;
    boilerManStartMillis += millis() + boilerHerinschakelWacht; // uitstel herinschakeling boiler na verbruikspiek
  }

  if (boilerManStartMillis > millis()){ // herinschakelwachttijd loopt nog
    boilerAan = false;
  }

  // schakeling relais boiler
  if (boilerAan){
    if (digitalRead(BOILERPIN) == LOW){  // tijdstip inschakeling boiler opslaan
      boilerStartMillis = millis(); 
      }
    digitalWrite(BOILERPIN, HIGH);
  }
  else{
    digitalWrite(BOILERPIN, LOW);
  }

  Serial.print("Boiler: ");
  Serial.println(boilerAan);


  //fietsoplader aansturing

  if ( millis() > boilerStartMillis + 5000 || boilerAan == false){  // meer dan 5 seconden na inschakelen boiler-relais
    if (vijfMinuGem > fietsAltijdAanWatt){ // voldoende injectie
      fietsAan = true;
    }

    if (solarMinuten > fietsSolarStartMin && PowerAfname < fietsInschakelWatt){ // klokgestuurd en lage afname
      fietsAan = true;
    }
  }

  if (vijfMinuGem < fietsAltijdAanWatt && minuten > fietsStopMin){ // onvoldoende injectie en klokgestuurd voorbij
    fietsAan = false;
  }

  if (PowerAfname > fietsUitschakelWatt){ // afname te hoog
    fietsAan = false;
  }

  // schakeling relais fietsoplader
  if (fietsAan){
    if (digitalRead(FIETSPIN) == LOW){ 
      fietsStartMillis = millis(); 
      }
    digitalWrite(FIETSPIN, HIGH);
  }
  else{
    digitalWrite(FIETSPIN, LOW);
  }

  Serial.print("Fietsoplader: ");
  Serial.println(fietsAan);
  Serial.println();
}