/*
Name:		SoilControl.ino
Created:	3/17/2017 4:09:02 PM
Author:	Kia Kaha
*/

#include <dht11\dht11.h> //air_humidity&temperature_sensor
#include <Thermistor\Thermistor.h> //NTC soil temperature sensor
#include <DS3231\DS3231.h> //RTC
#include <LiquidCrystal_I2C.h> //LCD display
#include <TimerOne-master\TimerOne.h> //PWM
#include <Wire.h>
#include <SD-master\SD.h>
#include <SPI\SPI.h>

#define NTCPIN A0 //soil_temperature_sensor
#define DHTPIN 8 //air_humidity&temperature_sensor
#define SDcard_CS 7 //ChipSelect PIN of the SD card
#define HEATERPIN 9 //thermistor - heating element
#define VALVEPIN 10 //electronic water valve
//#define SDcard_MOSI 11 //SD card
//#define SDcard_MISO 12 // SD card
#define SDcard_CLK 13 //SD card - Clock

dht11 dht;
Thermistor ntc1(NTCPIN);
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
DS3231  rtc(SDA, SCL);

//Settings
int waterAmount = 3000;
int withLCD = 1, i;
bool needsWater;
Time wateringScheduledFor;
int timen[2];
double Setpoint, Input, Output, invOut, inOld;
double  e = 0, eOld = 0, uOld = 0, q0, q1, q2, q3;
//Files
File myFile;
const char* fileWateringSchedule = "WtrSch.txt";
const char* logFile = "log.txt";

void setup() {
	rtc.begin();
	initilizeSDcard(SDcard_CS);
	//myFile  = SD.open("Sensors.txt", FILE_WRITE);
	//myFile.close();
	lcd.begin(16, 2);
	lcd.backlight();

	//showAds();
	Input = 0;
	Setpoint = 27;
	q0 = 1.2;
	q1 = -1.07;
	q2 = 0.3935;
	q3 = 0.6065;
	inOld = (29 - q2 * 26) / q3;
	eOld = 0;   i = 1 * 60;
	Timer1.initialize(500000);         // initialize timer1, and set a 1/2 second period
	Timer1.pwm(HEATERPIN, 1024);
	Timer1.pwm(VALVEPIN, 1024);
	Serial.begin(9600);
	//Watering
	if (SD.exists((char*)fileWateringSchedule)) {
		myFile = SD.open(fileWateringSchedule);
		if (myFile) {
			//wateringScheduledFor = ConvertStrToTime(readLine(myFile));
			//ConvertStrToTime(readLine(myFile));
			myFile.close();
		}
	}
	else {
		//needsWater = false;
		wateringScheduledFor = rtc.getTime();
	}
}

void loop() {
	//Gather data
	int chk = dht.read(DHTPIN);
	float airHum = (float)dht.humidity;
	float airTemp = (float)dht.temperature;
	double soilTemp1 = ntc1.getTemp();

	//Control Heating
	Input = q3*inOld + q2*soilTemp1;
	inOld = Input;
	if (i >= 1 * 60) {
		e = Setpoint - Input;
		Output = q0*e + q1*eOld + uOld;
		if (Output < 0) {
			Output = 0;
		}
		else if (Output > 30) {
			Output = 30;
		}
		uOld = Output;
		eOld = e;

		invOut = 1024 - Output*34.13;
		Timer1.pwm(9, invOut);
		i = 1;
	}

	//Control Watering	
	//if (IsItTimeToWater()) {
	WaterPlant(VALVEPIN);
	SetNextWatering();
	myFile = SD.open(fileWateringSchedule);
	if (myFile) {
		//wateringScheduledFor = ConvertStrToTime(readLine(myFile));
		ConvertStrToTime(readLine(myFile));
		myFile.close();
	}

	//}

	//Write Data on the SD
	if (myFile) {
		myFile = SD.open("Sensors.txt", FILE_WRITE);
		String fileText = "Air Temperature: " + (String)airTemp + ", Air Humidity: " + (String)airHum;
		myFile.println(fileText);
		myFile.close();
		Serial.println("Data written.");
	}

	//Print data
	if (withLCD) {
		lcd.clear();
		lcd.print("Input: "); lcd.print(Output);
		delay(2000);

		lcd.clear();
		lcd.print("Date: "); lcd.print(rtc.getDateStr());
		lcd.setCursor(0, 1);  lcd.print(rtc.getDOWStr()); lcd.print("  ");
		lcd.print(rtc.getTimeStr()); delay(2000);

		lcd.clear();
		lcd.print("Air Hum : "); lcd.print(airHum);
		lcd.print("%");
		lcd.setCursor(0, 1); delay(1000);
		lcd.print("Air Temp: "); lcd.print(airTemp, 1);
		lcd.print("C");

		delay(2000); lcd.clear();
		lcd.print("Soil Hum : 70%");
		lcd.setCursor(0, 1); delay(1000);
		lcd.print("Soil Temp:");
		lcd.print((float)soilTemp1, 1); lcd.print("C"); delay(2000);
		//showAds();
	}
	else {
		Serial.print("\nHumidity (%): ");  Serial.println((float)dht.humidity, 2);
		Serial.print("Temperature (?C): "); Serial.println((float)dht.temperature, 2);
		double soilTemp1 = ntc1.getTemp();
		Serial.print("Soil temperature (?C): ");
		Serial.println(soilTemp1);
	}//Print data

	i = i + 1;
	delay(5000);
}
/*
void showAds() {
lcd.setCursor(0, 0);   lcd.print("Plant Care v1.0");
delay(1000);
lcd.setCursor(0, 1);  lcd.print("   MADE IN BG!");
delay(2000);
}*/

void initilizeSDcard(int csPin) {
	pinMode(csPin, OUTPUT);

	if (!SD.begin(csPin)) {
		Serial.println("SD card init failed");
	}
}
String readLine(File file) {
	String received = "";
	char ch;
	while (file.available())
	{
		ch = file.read();
		if (ch == '\n')
		{
			return String(received);
		}
		else
		{
			received += ch;
		}
	}
	return "";
}
boolean IsItTimeToWater(Time givenTime) {
	Time now = rtc.getTime();

	if (now.date >= givenTime.date) {
		if (now.hour >= givenTime.hour) {
			if (now.min > givenTime.min) {
				return true;
				Serial.println("NeedsWater");
			}
		}
	}
	return false;
}
void WaterPlant(int valvePin) {
	Timer1.pwm(valvePin, 0);
	Serial.println("Start watering at " + (String)rtc.getTimeStr());
	delay(waterAmount);
	//Serial.println("Finish watering at " + (String)rtc.getTimeStr());
	Timer1.pwm(valvePin, 1024);
}
void SetNextWatering() {
	SD.remove((char*)fileWateringSchedule);
	myFile = SD.open(fileWateringSchedule, FILE_WRITE);
	if (myFile) {
		String nextDate = IncDate(rtc.getTime(), 0); //TODO
		String text = nextDate + "/" + (String)rtc.getTimeStr();
		myFile.println(text);
		myFile.close();
		Serial.println("Next WtrSch set at: " + text);
	}
	else {
		Serial.println("Failed to set next WtrSch");
	}
}
String IncDate(Time date, int days) {
	date.date += days;
	String newDate = (String)date.date + "." + (String)date.mon + "." + (String)date.year;
	return newDate;
}
Time ConvertStrToTime(String timeStr) {
	int arr[5];
	Time result;
	int index = 0;
	
	for (int i = 0; i < 5; i++) {
		if (i < 2) index = timeStr.indexOf(".");
		else if (i == 2) index = timeStr.indexOf("/");
		else  index = timeStr.indexOf(":");

		arr[i] = timeStr.substring(0, index).toInt();
		timeStr = timeStr.substring(index + 1, timeStr.length());
		//Serial.println(arr[i]);
	}
	result.sec = timeStr.toInt();
	result.min = arr[4];
	result.hour = arr[3];
	result.year = arr[2];
	result.mon = arr[1];
	result.date = arr[0];
	return result;
}
void logInsertLn() {
	//String fileText = "Air Temperature: " + (String)airTemp + ", Air Humidity: " + (String)airHum;

}