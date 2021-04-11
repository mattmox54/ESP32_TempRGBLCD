#include <WiFi.h>
#include <LiquidCrystal.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define MAX_ULONG_VALUE 4294967296

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, -14396); //4hrs->seconds = 14400, 4 sec shift to match windows clock

const int BUTTON_PIN = 35;

int button_flag = 0;
unsigned long StartTime = 0;

const char* ssids[]     = {
  "", /*HERE*/
  "",
  ""
};

const char* passwords[] = {
  "", /*HERE*/
  "",
  ""
};

const int NUM_WIFI_NETWORKS = 3;

const char* api_id = ""; /*HERE*/
const char* server = "api.openweathermap.org";   

WiFiClient client;

const int rs = 14, en = 27, d4 = 32, d5 = 33, d6 = 25, d7 = 26;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

char temp_str[32] = "";

const int GreenLED = 13;//12
const int BlueLED = 15;//13
const int RedLED = 12;

const int PWM_FREQ = 1000;
const int PWM_RESOLUTION = 8;
const int PWM_CHANNEL_GREEN = 0;
const int PWM_CHANNEL_BLUE = 1;
const int PWM_CHANNEL_RED = 2;

struct location {
  const char* name;
  const char* lon;
  const char* lat;
};

struct location locations[] = {
  {
    .name = "Pittsburgh     ",
    .lon = "-80.070783",
    .lat = "40.407479", 
  },
  {
    .name = "Mercer         ",
    .lon = "-80.188594",
    .lat = "41.270616", 
  },
  {
    .name = "Kokomo         ",
    .lon = "-86.101843",
    .lat = "40.436180", 
  }
};

const int num_locations = 3;
int LocationIndex = 0;

void IRAM_ATTR interrupt_function(){
  button_flag = 1;
}

void setup() {
    Serial.begin(115200);
    delay(10);

    // set up the LCD's number of columns and rows:
    lcd.begin(16, 2);

    WifiSetup();

    lcd.clear();
    lcd.print(locations[LocationIndex].name);

    ledcSetup(PWM_CHANNEL_GREEN, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(GreenLED, PWM_CHANNEL_GREEN);
  
    ledcSetup(PWM_CHANNEL_BLUE, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(BlueLED, PWM_CHANNEL_BLUE);

    ledcSetup(PWM_CHANNEL_RED, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(RedLED, PWM_CHANNEL_RED);

    ledcWrite(PWM_CHANNEL_RED, 255);

    pinMode(BUTTON_PIN, INPUT); 
  
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), interrupt_function, FALLING);

    timeClient.begin();

    getWeather();
    setLCD();
}

void WifiSetup(void){
    int wifi_connected_flag = 0;
    int index = 0;
    int max_connect_tries = 6;
    int num_tries = 0;

    while(wifi_connected_flag==0){

        lcd.clear();
        lcd.print("Connecting to");
        lcd.setCursor(0, 1);
        lcd.print(ssids[index]);
        
        WiFi.begin(ssids[index], passwords[index]);

        num_tries = 0;
        while(WiFi.status() != WL_CONNECTED && num_tries<=max_connect_tries){
            delay(500);
            num_tries++;
            Serial.print(".");
        }
    
        if(WiFi.status() != WL_CONNECTED) {
            //wifi not connected
            Serial.print(".");
            index++; 
            if(index==NUM_WIFI_NETWORKS){
                index = 0;          
            }
        }else{
            //wifi connnected
            wifi_connected_flag = 1;
        }
    }

    Serial.println("");
    Serial.println("WiFi connected");
}

void loop() {
    unsigned long current_time = 0;

    if(button_flag==1){
        Serial.println("button pressed");
        delay(600);
        button_flag = 0;
        
        UpdateLocation();
    }

    current_time = millis();

    if(current_time<StartTime){
      //ROLL OVER
      if((MAX_ULONG_VALUE-StartTime+current_time)>10000){
        getWeather();
        setLCD();
        StartTime = millis();  
      }   
    }else{
      //NORMAL OPERATION
      if((current_time-StartTime)>10000){
        getWeather();
        setLCD();
        StartTime = millis();  
      }
    }
    
    delay(10);
}

void UpdateLocation(){

  LocationIndex++;
  if(LocationIndex>=num_locations)
    LocationIndex = 0;

  lcd.setCursor(0, 0);
  lcd.print(locations[LocationIndex].name);

  getWeather();
  setLCD();

}

void getTimeStr(char time_str[]){
  int hours = 0;
  int minutes = 0;
  
  timeClient.update();

  hours = timeClient.getHours();
  minutes = timeClient.getMinutes();

  sprintf(time_str, "%02d:%02d %s", hours>12?(hours-12):hours, minutes, hours>11?"PM":"AM");
}

void setLCD(){
  double temp_dbl = 0;
  int y = 0;
  int green_duty_cycle = 0;
  int blue_duty_cycle = 0;
  int hours = 0;
  char time_str[32] = "";
  char temp_w_units[32] = "";

  lcd.setCursor(0, 1);

  sprintf(temp_w_units, "%s%cF", temp_str, (char)223);

  lcd.print(temp_w_units);
  lcd.print(" ");
  getTimeStr(time_str);
  lcd.print(time_str);
  temp_dbl = atof(temp_str);

  y = (int)(13.42*temp_dbl - 429.474);

  green_duty_cycle = 255-y;
  
  if(green_duty_cycle>255){
    green_duty_cycle = 255;
  }else if(green_duty_cycle<0){
    green_duty_cycle = 0;
  }

  blue_duty_cycle = y-255;

  if(blue_duty_cycle>255){
    blue_duty_cycle = 255;
  }else if(blue_duty_cycle<0){
    blue_duty_cycle = 0;
  }

  hours = timeClient.getHours();

  if(hours>=23 || hours<6){
    //Night time, turn off backlight
    ledcWrite(PWM_CHANNEL_GREEN, 255);
    ledcWrite(PWM_CHANNEL_BLUE, 255);
  }else{
    ledcWrite(PWM_CHANNEL_GREEN, green_duty_cycle);
    ledcWrite(PWM_CHANNEL_BLUE, blue_duty_cycle); 
  }
  
  //timeClient.update();
  //Serial.println(timeClient.getFormattedTime());
}

void getWeather() { 
    String line_str = "";
	  char line[4096] = "";
    char * temp_str_start = nullptr;
	  char * temp_str_end = nullptr;
    char get_request[512] = "";

    sprintf(get_request, "GET /data/2.5/weather?lat=%s&lon=%s&appid=%s&units=imperial", locations[LocationIndex].lat, locations[LocationIndex].lon, api_id);

    Serial.println("\nStarting connection to server..."); 
    // if you get a connection, report back via serial: 
    if (client.connect(server, 80)) { 
        Serial.println("connected to server"); 
        // Make a HTTP request: 
        client.println(get_request); 
        client.println("Connection: close"); 
        client.println(); 
    } else { 
        Serial.println("unable to connect"); 
    } 
    delay(1000);  
    while (client.connected()) { 
        line_str = client.readStringUntil('\n'); 
		    line_str.toCharArray(line, 4096);
        //Serial.println(line); 
        temp_str_start = strstr(line, "\"temp\":"); //find "temp":
        if(temp_str_start==nullptr){
			      Serial.println("ERROR: Counldn't find temp");
		    }else{
			      temp_str_end = strchr(temp_str_start, ',');
			      if(temp_str_end==nullptr){	
				        Serial.println("ERROR: Counldn't find comma");
			      }else{
                temp_str_start = strchr(temp_str_start, ':');
                if(temp_str_start==nullptr){
                    Serial.println("ERROR: Counldn't find colon");
                }else{
                    temp_str_start++;
                    strncpy(temp_str, temp_str_start, temp_str_end-temp_str_start-1);
                    temp_str[temp_str_end-temp_str_start-1] = '\0';
                    Serial.println(temp_str);
                }
			     }
		    }      
    }

    client.stop();
} 
