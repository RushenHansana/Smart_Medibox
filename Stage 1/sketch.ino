#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
//WiFi Timing
#include <WiFi.h>
#include <SPI.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
 
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS  "1.asia.pool.ntp.org"
WiFiUDP ntpUDP;


//Display
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1 //Reset pin number 
#define SCREEN_ADDRESS 0x3c
//pin definition
#define BUZZER 5
#define LED_1 15
#define PB_CANCEL 34 //Pullup resistor
#define PB_OK 32 //Pullup resistor
#define PB_UP 33 //Pullup resistor
#define PB_DOWN 35 //Pullup resistor
#define DHTPIN 12
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;

//Global variables
float NTP_OFFSET = 19800; // In seconds
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;


//For time calculations
unsigned long last_time = 0;
unsigned long now_time = 0;

//For alarm
bool alarm_enabled = true;
int n_alarms = 3;
int alarm_hours[] = {0,1};
int alarm_minutes[] = {1, 10};
bool alarm_triggered[] = {false, false, false};

//For buzzer
int n_notes = 8; 
int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;
int notes[] = {C, D, E, F, G, A, B, C_H};

int current_mode = 0;
int max_modes = 5;
String modes[] = {"1 - Set Time Zone", "2 - Set Alarm 1", "3 - Set Alarm 2", "3 - Set Alarm 3", "4 - Disable Alarms"};


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);
  
  dhtSensor.setup(DHTPIN, DHTesp::DHT22);
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)){

    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, Loop forever
  }
  //Connect to Wi
  Serial.print("Connecting to WiFi");
  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
    
    
  }
  Serial.println(" Connected!");
  // show initial display buffer contents on the screen
  // the library initializes this width an adafruit splash screen
  display.display();

  // Clear the buffer
  display.clearDisplay();
  Serial.println("Menu Button is little unresponsive.Please press and hold liitle bit to get menu.Cannot show temp & humidity warnings on display due to wokwi compilation timeout.");
  

  
}

void loop() {
  //to update time and facilitate timezone update
  NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);
  timeClient.begin();
  display.clearDisplay();
  timeClient.update();
  String formattedTime = timeClient.getFormattedTime();
  print_line("Smart medibox",1,0,20);
  print_line(formattedTime,2,20,15);
  if (digitalRead(PB_OK) == LOW){
    delay(100);
    go_to_menu();
    }
  String hour = formattedTime.substring(0, 2);
  String minute = formattedTime.substring(3, 5);
  hours = hour.toInt();
  minutes = minute.toInt();

  check_alarm();
  
  //Check temperature
  check_temp();
  digitalWrite(LED_1, LOW);
}

void print_line (String text, int text_size, int row, int column) {
  //Display a custom message
  display.setTextSize(text_size); // Normal 1:1 pixel scale.
  display.setTextColor (SSD1306_WHITE); // Draw white text
  display.setCursor(column, row); // start at (row, column)
  display.println(text);

  //Display on OLED dispplay
  display.display();
}
void check_temp(){
  
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  //bool all_good = true; //From lecture slide

  if (data.temperature > 32){
    //display.clearDisplay();
    //print_line("TEMP HIGH!", 1, 40, 0);
    //delay(200);
    digitalWrite(LED_1, HIGH);
  }

  else if (data.temperature < 26){
    //display.clearDisplay();
    //("TEMP LOW!", 1, 40, 0);
    //delay(200);
    digitalWrite(LED_1, HIGH);
  }

  if (data.humidity > 80){
    //display.clearDisplay();
    //print_line("HUMIDITY HIGH!", 1, 60, 0);
    //delay(200);
    digitalWrite(LED_1, HIGH);
  }

  else if (data.humidity < 60){
    //display.clearDisplay();
    //print_line("HUMIDITY LOW!", 1, 60, 0);
    //delay(200);
    digitalWrite(LED_1, HIGH);
  }

  /*if (all_good) { //From lecture slide
    digitalWrite(LED_2, LOW);
  } */
}

void ring_alarm(){
display.clearDisplay();
print_line("MEDICINE", 2, 0, 20);
print_line("TIME!", 2, 40, 40);

//Turn ON LED_1
digitalWrite(LED_1, HIGH);


bool break_happened = false;

//Ring the buzzer
while (break_happened == false && digitalRead(PB_CANCEL) == HIGH){
    for (int i=0; i<n_notes; i++){
      if (digitalRead(PB_CANCEL) == LOW) {
        delay(200); //To avoid bouncing
        break_happened = true; //To exit from outside while loop
        break;
      }

      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(2);
  }
}
}
void run_mode(int mode){
  if (mode == 0){
    set_time();
  }

  else if (mode == 1 || mode == 2 ){
    set_alarm(mode-1);
  }

  else if (mode == 3 ){
    set_alarm(mode-1);
  }

  else if (mode == 4){
    disable_all_alarms();
    
    //enable alarms when alarm set option is given
  }
}
int wait_for_button_press(){
  while(true){
    if (digitalRead(PB_UP) == LOW){
      delay(200);
      return PB_UP;
    }

  else if (digitalRead(PB_DOWN) == LOW){
      delay(200);
      return PB_DOWN;
    }

  else if (digitalRead(PB_OK) == LOW){
      delay(200);
      return PB_OK;
    }
  
  else if (digitalRead(PB_CANCEL) == LOW){
      delay(200);
      return PB_CANCEL;
    }

    //update_time();
  }
}
////////////////////////////////////////////////////////////////////////////
void go_to_menu(){
  while(digitalRead(PB_CANCEL) == HIGH){
    display.clearDisplay();
    print_line(modes[current_mode], 2, 0, 0);

    int pressed = wait_for_button_press();

    if (pressed == PB_UP) {
      delay(200);
      current_mode += 1;
      current_mode = current_mode % max_modes;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      current_mode -= 1;
      if (current_mode<0){
        current_mode = max_modes-1;
      }
    }

    else if (pressed == PB_OK) {
      Serial.println(current_mode);
      delay(200);
      run_mode(current_mode);
    }

    else if (pressed == PB_CANCEL){
      delay(200);
      break;
    }

    //update_time();
  }
}
//seting up timezone
void set_time(){
  float timezone = 5.5;
  while (true){
    display.clearDisplay();
    print_line("Enter timezone: " + String(timezone), 2, 0, 0);

    int pressed = wait_for_button_press();
      if (pressed == PB_UP) {
        delay(200);
         timezone+= 0.5;
        if (timezone>14){
          timezone = -12;
        }
      }

      else if (pressed == PB_DOWN) {
        delay(200);
        timezone-= 0.5;
          if (timezone<-12){
          timezone = 14;
        }
      }

      else if (pressed == PB_OK) {
        delay(200);
        NTP_OFFSET= timezone*3600;
        display.clearDisplay();
        print_line(" Timezone Upadted", 2, 20, 10);
        delay(200);
        break;
      }

      else if (pressed == PB_CANCEL){
        NTP_OFFSET = 19800;
        delay(200);
        break;
      } 
  }

  
}
//setting alarm times
void set_alarm(int alarm){
  //Setting hours
  int temp_hour = alarm_hours[alarm];
  
  while (true){
    display.clearDisplay();
    print_line("Enter hour: " + String(temp_hour), 2, 0, 0);

    int pressed = wait_for_button_press();
      if (pressed == PB_UP) {
        delay(200);
        temp_hour += 1;
        temp_hour = temp_hour % 24;
      }

      else if (pressed == PB_DOWN) {
        delay(200);
        temp_hour -= 1;

        if (temp_hour<0){
          temp_hour = 23;
        }
      }

      else if (pressed == PB_OK) {
        delay(200);
        alarm_hours[alarm] = temp_hour;
        break;
      }

      else if (pressed == PB_CANCEL){
        display.clearDisplay();
        delay(200);
        break;
      }

    //update_time();
  } 

  //Setting minutes
  int temp_minutes = alarm_minutes[alarm];
  
  while (true){
    display.clearDisplay();
    print_line("Enter minutes: " + String(temp_minutes), 2, 0, 0);

    int pressed = wait_for_button_press();
      if (pressed == PB_UP) {
        delay(200);
        temp_minutes += 1;
        temp_minutes = temp_minutes % 60;
      }

      else if (pressed == PB_DOWN) {
        delay(200);
        temp_minutes -= 1;

        if (temp_minutes<0){
          temp_minutes = 59;
        }
      }

      else if (pressed == PB_OK) {
        delay(200);
        alarm_minutes[alarm] = temp_minutes;
        break;
      }

      else if (pressed == PB_CANCEL){
        delay(200);
        break;
      }

    //update_time();
  } 

  display.clearDisplay();
  print_line("Alarm is set", 2, 0, 0);
  delay(1000);
}
//check alarm times
void check_alarm(){

  //Check for alarams
  if (alarm_enabled){
    //Iterating through all alarms
    for (int i=0; i<n_alarms; i++){
      //alarm_triggered[i] == false???????????????????????
      if (alarm_triggered[i] == false && alarm_hours[i] == hours && alarm_minutes[i] == minutes){
        ring_alarm();
        alarm_triggered[i] = true;
      }
    }
  }
}
//Interface for disabling all alarms
void disable_all_alarms(){
  while (true){
    display.clearDisplay();
    print_line("Disable All Alarms", 2, 0, 0);
    int pressed = wait_for_button_press();
    if (pressed == PB_OK) {
        delay(200);
        alarm_enabled = false;
        display.clearDisplay();
        print_line(" All OFF", 2, 0, 0);
        delay(200);
        break;
      }

    else if (pressed == PB_CANCEL){
        delay(200);
        display.clearDisplay();
        print_line(" All ON", 2, 0, 0);
        alarm_enabled = true;
        delay(200);
        break;
      }
  }

}