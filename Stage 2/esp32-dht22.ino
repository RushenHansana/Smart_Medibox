#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// OLED parameters
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0X3C

#define NTP_SERVER "pool.ntp.org"
#define UTC_OFFSET 0
#define UTC_OFFSET_DST 0

#define BUZZER 5
#define LED_1 15
#define PB_Cancel 34
#define PB_OK 32
#define PB_Up 33
#define PB_Down 35
#define DHTpin 12
const int LDRpin = 36;
const int servoPin = 18;

// Declare objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;

WiFiClient espClient;
PubSubClient mqttClient(espClient);
Servo servo;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

unsigned long time_now = 0;
unsigned long time_last = 0;

bool alarm_enabled = true;
int n_alarm = 3;
int alarm_hours[3] = {0, 1, 2};
int alarm_minutes[3] = {0, 10, 12};
bool alarm_active[3] = {false, false, false};

int n_notes = 8;
int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int CH = 523;
int notes[8] = {C, D, E, F, G, A, B, CH};

int current_mode = 0;
int max_modes = 5;
String modes[] = {"1-Set Time", "2-Set Alarm 1", "3-Set Alarm 2", "4-Set Alarm 3", "5-Disable Alarms"};
char tempAr[6]="30";
char LDR_chr[6];
float LDR_val=0.0;
int minAngle;
int crlFact;
int theta=0;
bool isSchedule=false;
unsigned long scheduleOnTime;

void setup()
{
    pinMode(BUZZER, OUTPUT);
    pinMode(LED_1, OUTPUT);
    pinMode(PB_Cancel, INPUT);
    pinMode(PB_OK, INPUT);
    pinMode(PB_Up, INPUT);
    pinMode(PB_Down, INPUT);

    dhtSensor.setup(DHTpin, DHTesp::DHT22);

    Serial.begin(9600);

    WiFi.begin("Wokwi-GUEST", "", 6);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(250);
        display.clearDisplay();
        print_line("Connecting to WIFI", 0, 0, 2);
    }

    display.clearDisplay();
    print_line("Connected to WIFI", 0, 0, 2);

    configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
    {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ;
    }

    display.display();
    delay(500);
    display.clearDisplay();
    print_line("Welcome to medibox", 10, 20, 2);
    display.clearDisplay();
    servo.attach(servoPin, 500, 2400);
    setupWifi();
    setupMqtt();
    timeClient.begin();
    timeClient.setTimeOffset(5.5*3600);

}

void loop()
{
    update_time_check_alarm();
    if (digitalRead(PB_OK) == LOW)
    {
        delay(200);
        go_to_menue();
    }
    check_temp();

    if (!mqttClient.connected())
    {
        connectToBroker();
    }

    LDR_val = analogRead(LDRpin)/4063.0;
    // Serial.println(LDR_val);
    updateTemp();
    // Serial.println(tempAr);
    String(LDR_val, 2).toCharArray(LDR_chr, 6);
    mqttClient.loop();
    mqttClient.publish("Intensity_ENTC", tempAr);
    mqttClient.publish("Temprature_ENTC", LDR_chr);
    

    checkSchedule();
    delay(1000);
    theta=minAngle + (180-minAngle)*crlFact*LDR_val;
    servo.write(theta);
    delay(50);

    Serial.println(theta);

}
void updateTemp(){
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    String(data.temperature, 2).toCharArray(tempAr, 6);
}

void checkSchedule(){
    if(isSchedule){
        unsigned long currentTime=getTime();
        if(currentTime>scheduleOnTime){
            buzzer_on(true);
            isSchedule=false;
            Serial.println("Scheduled ON");
            mqttClient.publish("OnOffESP", "1");
            mqttClient.publish("SchESPOn", "0");
        }
    }
}

unsigned long getTime(){
    timeClient.update();
    return timeClient.getEpochTime();
}

void buzzer_on(bool on){
    if(on){
        tone(BUZZER, 255);

            
    }else{
        noTone(BUZZER);
    }
}


void connectToBroker()
{
    while (!mqttClient.connected())
    {
        Serial.println("Attempting MQTT connect");
        if (mqttClient.connect("ESP32-4546"))
        {
            Serial.println("connected");
            mqttClient.subscribe("M.A");
            mqttClient.subscribe("C.F");
            mqttClient.subscribe("Buzzer");
            mqttClient.subscribe("SwitchedOn");
            
        }
        else
        {
            Serial.print("failed");
            Serial.print(mqttClient.state());
            delay(5000);
        }
    }
}

void setupWifi()
{
    WiFi.begin("Wokwi-GUEST", "");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("Wifi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void setupMqtt()
{
    mqttClient.setServer("test.mosquitto.org", 1883);
    mqttClient.setCallback(receiveCallback);
}

void receiveCallback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arriced [");
    Serial.print(topic);
    Serial.print("] ");

    char payloadCharAr[length];
    for (int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
        payloadCharAr[i] = (char)payload[i];
    }

    Serial.println();

    if (strcmp(topic, "M.A") == 0)
    {

        minAngle = atoi(payloadCharAr);
    }
    else if (strcmp(topic, "C.F") == 0)
    {

        crlFact = atoi(payloadCharAr);
    }else if(strcmp(topic,"Buzzer")==0){
          buzzer_on(payloadCharAr[0]=='1'  );
        
    }else if(strcmp(topic,"SwitchedOn")==0){
          if(payloadCharAr[0]=='N'){
            isSchedule=false;
          }else{
            isSchedule=true;
scheduleOnTime=atol(payloadCharAr);
          }
        
    }

}

void print_line(String text, int col, int row, int txt_size)
{
    display.setTextSize(txt_size);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(col, row);
    display.println(text);
    display.display();
}

void print_time_now(void)
{
    display.clearDisplay();
    print_line(String(days), 0, 0, 2);
    print_line(":", 20, 0, 2);
    print_line(String(hours), 30, 0, 2);
    print_line(":", 50, 0, 2);
    print_line(String(minutes), 60, 0, 2);
    print_line(":", 80, 0, 2);
    print_line(String(seconds), 90, 0, 2);
}

void update_time()
{
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    char timeHour[3];
    strftime(timeHour, 3, "%H", &timeinfo);
    hours = atoi(timeHour);

    char timeMinute[3];
    strftime(timeMinute, 3, "%M", &timeinfo);
    minutes = atoi(timeMinute);

    char timeSecond[3];
    strftime(timeSecond, 3, "%S", &timeinfo);
    seconds = atoi(timeSecond);

    char timeDay[3];
    strftime(timeDay, 3, "%d", &timeinfo);
    days = atoi(timeDay);
    // time_now = millis() / 1000;
    // seconds = time_now - time_last;
    // if (seconds >= 60)
    // {
    //     minutes++;
    //     time_last += 60;
    // }

    // if (minutes == 60)
    // {
    //     hours++;
    //     minutes = 0;
    // }

    // if (hours == 24)
    // {
    //     days++;
    //     hours = 0;
    // }
}

void ring_alarm()
{
    display.clearDisplay();
    print_line("Medicine time!", 0, 0, 2);
    digitalWrite(LED_1, HIGH);

    bool break_happened = false;

    while (break_happened == false && digitalRead(PB_Cancel) == HIGH)
    {
        for (int i = 0; i < n_notes; i++)
        {
            if (digitalRead(PB_Cancel) == LOW)
            {
                delay(200);
                break_happened = true;
                break;
            }
            tone(BUZZER, notes[i]);
            delay(500);
            noTone(BUZZER);
            delay(2);
        }
    }

    digitalWrite(LED_1, LOW);
    display.clearDisplay();
}

void update_time_check_alarm(void)
{
    update_time();
    print_time_now();

    if (alarm_enabled)
    {
        for (int i = 0; i < n_alarm; i++)
        {
            if (alarm_hours[i] == hours && alarm_minutes[i] == minutes && alarm_active[i] == false)
            {
                ring_alarm();
                alarm_active[i] = true;
            }
        }
    }
}

int wait_for_button_pressed(void)
{
    while (true)
    {
        if (digitalRead(PB_Up) == LOW)
        {
            delay(200);
            return PB_Up;
        }

        if (digitalRead(PB_Down) == LOW)
        {
            delay(200);
            return PB_Down;
        }

        if (digitalRead(PB_OK) == LOW)
        {
            delay(200);
            return PB_OK;
        }

        if (digitalRead(PB_Cancel) == LOW)
        {
            delay(200);
            return PB_Cancel;
        }

        update_time();
    }
}

void go_to_menue(void)
{
    while (digitalRead(PB_Cancel) == HIGH)
    {
        display.clearDisplay();
        print_line(modes[current_mode], 0, 0, 2);

        int pressed = wait_for_button_pressed();
        if (pressed == PB_Up)
        {
            delay(200);
            current_mode += 1;
            current_mode %= max_modes;
        }

        else if (pressed == PB_Down)
        {
            delay(200);
            current_mode -= 1;
            if (current_mode < 0)
            {
                current_mode = max_modes - 1;
            }
        }

        else if (pressed == PB_OK)
        {
            delay(200);
            run_mode(current_mode);
        }

        else if (pressed == PB_Cancel)
        {
            delay(200);
            break;
        }
    }
}

void set_time()
{
    int temp_hour = hours;
    while (true)
    {
        display.clearDisplay();
        print_line("Enter hour: " + String(temp_hour), 0, 0, 2);
        int pressed = wait_for_button_pressed();
        if (pressed == PB_Up)
        {
            delay(200);
            temp_hour += 1;
            temp_hour %= 24;
        }

        else if (pressed == PB_Down)
        {
            delay(200);
            temp_hour -= 1;
            if (temp_hour < 0)
            {
                temp_hour = 23;
            }
        }

        else if (pressed == PB_OK)
        {
            delay(200);
            hours = temp_hour;
            break;
        }

        else if (pressed == PB_Cancel)
        {
            delay(200);
            break;
        }
    }

    int temp_minute = minutes;
    while (true)
    {
        display.clearDisplay();
        print_line("Enter minute: " + String(temp_minute), 0, 0, 2);
        int pressed = wait_for_button_pressed();
        if (pressed == PB_Up)
        {
            delay(200);
            temp_minute += 1;
            temp_minute %= 60;
        }

        else if (pressed == PB_Down)
        {
            delay(200);
            temp_minute -= 1;
            if (temp_minute < 0)
            {
                temp_minute = 59;
            }
        }

        else if (pressed == PB_OK)
        {
            delay(200);
            minutes = temp_minute;
            break;
        }

        else if (pressed == PB_Cancel)
        {
            delay(200);
            break;
        }
    }

    display.clearDisplay();
    print_line("Time set!", 0, 0, 2);
    delay(200);
}

void set_alarm(int alarm)
{
    int temp_hour = alarm_hours[alarm];
    while (true)
    {
        display.clearDisplay();
        print_line("Enter hour: " + String(temp_hour), 0, 0, 2);
        int pressed = wait_for_button_pressed();
        if (pressed == PB_Up)
        {
            delay(200);
            temp_hour += 1;
            temp_hour %= 24;
        }

        else if (pressed == PB_Down)
        {
            delay(200);
            temp_hour -= 1;
            if (temp_hour < 0)
            {
                temp_hour = 23;
            }
        }

        else if (pressed == PB_OK)
        {
            delay(200);
            alarm_hours[alarm] = temp_hour;
            break;
        }

        else if (pressed == PB_Cancel)
        {
            delay(200);
            break;
        }
    }

    int temp_minute = alarm_minutes[alarm];
    while (true)
    {
        display.clearDisplay();
        print_line("Enter minute: " + String(temp_minute), 0, 0, 2);
        int pressed = wait_for_button_pressed();
        if (pressed == PB_Up)
        {
            delay(200);
            temp_minute += 1;
            temp_minute %= 60;
        }

        else if (pressed == PB_Down)
        {
            delay(200);
            temp_minute -= 1;
            if (temp_minute < 0)
            {
                temp_minute = 59;
            }
        }

        else if (pressed == PB_OK)
        {
            delay(200);
            alarm_minutes[alarm] = temp_minute;
            break;
        }

        else if (pressed == PB_Cancel)
        {
            delay(200);
            break;
        }
    }

    display.clearDisplay();
    print_line("Alarm set!", 0, 0, 2);
    delay(200);
}

void run_mode(int mode)
{
    if (mode == 0)
    {
        set_time();
    }

    else if (mode == 1 || mode == 2 || mode == 3)
    {
        set_alarm(mode - 1);
    }

    else if (mode == 4)
    {
        alarm_enabled = false;
    }
}

void check_temp()
{
TempAndHumidity data = dhtSensor.getTempAndHumidity();
    
    if (data.temperature > 33)
    {
        display.clearDisplay();
        print_line("Temperature high!", 0, 40, 1);
    }

    else if (data.temperature < 25)
    {
        display.clearDisplay();
        print_line("Temperature low!", 0, 50, 1);
    }

    if (data.humidity > 80)
    {
        display.clearDisplay();
        print_line("Humidity high!", 0, 50, 1);
    }

    else if (data.humidity < 60)
    {
        display.clearDisplay();
        print_line("Humidity low!", 0, 50, 1);
    }
}