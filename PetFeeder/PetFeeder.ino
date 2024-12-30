#include <Ultrasonic.h>
#include <math.h>
#include <Grove_LED_Bar.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h> 
#include <Servo.h>

// ========== WiFi + ThingSpeak ========== 
#include <LWiFi.h>

// WiFi 相關
char ssid[] = "han";
char pass[] = "0906655956";
int status = WL_IDLE_STATUS;
WiFiClient client;

// ThingSpeak 相關
char server[] = "api.thingspeak.com";  
String writeAPIKey = "J9DJEB7TVP26VTAK";

//---------------常數、物件宣告---------------
#define LIGHT_SENSOR_PIN A0
Ultrasonic ultrasonic(2);
Grove_LED_Bar bar(5, 4, 0);
Servo myservo;
LiquidCrystal_I2C lcd(0x27);

//---------------全域變數設定---------------
int pos = 0;                       // variable to store the servo position
const int MAX_HEIGHT = 30;         // 飼料槽總高度（公分）(待設定)
const int threshold = 300;         // 光感差閾值(待設定)
int previousReading = 0;           // 紀錄上一個光感測器讀值
unsigned long messageStartTime = 0; 
const unsigned long displayDuration = 5000; // LCD 訊息顯示持續時間（毫秒）
bool displayMessage = false;                // 是否正在顯示訊息

//---------------投餵時間用---------------
unsigned long previousFeedMillis = 0;       
const unsigned long feedInterval = 30000;   // 測試階段：30 秒投餵一次
bool isFeeding = false;                     // 是否正在投食

//---------------飲水狀況用---------------
unsigned long waterCheckStart = 0;          
const unsigned long waterCheckInterval = 120000; // 2 分鐘偵測週期
int lastWaterLevel = -1;                    
bool waterChanged = false;                  

//---------------上傳參數用---------------
int feederLevelData = 0;    // 要上傳到 ThingSpeak 的飼料槽剩餘量 (%)

// 時間控制(避免太密集上傳)
unsigned long lastUpdateTS = 0;      
const unsigned long updateInterval = 20000; // 每 20 秒上傳一次(示範)

void setup() {
    Serial.begin(9600);
    bar.begin();
    lcd.begin(16, 2);
    lcd.backlight(); 
    lcd.setCursor(0,0);
    lcd.print("Awaiting Input...");
    myservo.attach(11);

    // 連線到 WiFi
    while (status != WL_CONNECTED) {
      Serial.print("Attempting to connect to SSID: ");
      Serial.println(ssid);
      status = WiFi.begin(ssid, pass);
      delay(1000);
    }
    Serial.println("Connected to WiFi!");
    printWifiStatus();
}

void loop() {
    detectMealTime();
    motorControl();
    detectFoodLevelWithUltrasonic();
    waterStatus();

    // 每 20 秒上傳一次
    unsigned long currentMillis = millis();
    if (currentMillis - lastUpdateTS >= updateInterval) {
        lastUpdateTS = currentMillis; 
        sendDataToThingSpeak(); // 上傳飼料槽百分比
    }
}

//--------------------------------------------------------------------------------------
//  Pet Feeder 原有函式 (略作調整: detectFoodLevelWithUltrasonic() 補存 feederLevelData)
//--------------------------------------------------------------------------------------
void detectMealTime() {
    int currentReading = analogRead(LIGHT_SENSOR_PIN);
    int lightReadingDiff = abs(currentReading - previousReading);

    if (lightReadingDiff > threshold && !displayMessage) {
      displayMessage = true;
      messageStartTime = millis(); 
      lcd.setCursor(0, 0); 
      lcd.print("Pet has eaten!   ");
      Serial.print("Current Light Sensor Reading: ");
      Serial.println(currentReading);
      Serial.print("Light Reading Difference: ");
      Serial.println(lightReadingDiff);
    }

    if (displayMessage) {
      if (millis() - messageStartTime >= displayDuration) {
        lcd.setCursor(0, 0); 
        lcd.print("                ");
        displayMessage = false;
      }
    }
    previousReading = currentReading; 
}

void motorControl() {
    unsigned long currentMillis = millis();

    if ((currentMillis - previousFeedMillis) >= feedInterval) {
        previousFeedMillis = currentMillis;
        
        int currentLight = analogRead(LIGHT_SENSOR_PIN);
        if (currentLight > 800) {
          Serial.println("== Feeding Time ==");
          isFeeding = true;
          for (pos = 0; pos <= 90; pos += 1) {
             lcd.setCursor(0, 1); 
             lcd.print("Feeding..."); 
             myservo.write(pos);
          }
          lcd.print("                ");
        }
    }

    if (isFeeding) {
        int currentLight = analogRead(LIGHT_SENSOR_PIN);
        if (currentLight <= 800) {
          for (pos = 90; pos >= 0; pos -= 1) {
              myservo.write(pos);
              lcd.setCursor(0, 1); 
              lcd.print("End Feeding..."); 
          }
          lcd.print("                ");
          isFeeding = false;
        }
    }
}

void detectFoodLevelWithUltrasonic() {
    long distance = ultrasonic.MeasureInCentimeters();
    if (distance > 0 && distance <= MAX_HEIGHT) {
      int remainingLevel = MAX_HEIGHT - distance; 
      int percentage = (remainingLevel * 100) / MAX_HEIGHT;
      ledBarHandler(percentage);

      // 將偵測到的百分比存到全域變數
      feederLevelData = percentage;

      if (percentage < 20) {
         lcd.setCursor(0, 0); 
         lcd.print("Refill food needed");
      }
    } else {
      feederLevelData = 0;
    }
}

void waterStatus() {
    unsigned long currentMillis = millis();
    if (waterCheckStart == 0) {
      waterCheckStart = currentMillis;
    }

    if ((currentMillis - waterCheckStart) >= waterCheckInterval) {
      waterCheckStart = currentMillis; 
      int currentWaterLevel = getWaterLevel();
      if (lastWaterLevel == -1) {
         lastWaterLevel = currentWaterLevel;
      } else {
         if (abs(currentWaterLevel - lastWaterLevel) > 5) {
            waterChanged = true;
            Serial.println("Water level changed, record the drinking time!");
         }
         lastWaterLevel = currentWaterLevel;
      }
    }
}

void ledBarHandler(int percentage) {
  if (percentage < 10) {
    bar.setBits(0b0000000001);
  } else if (percentage >= 10 && percentage < 20) {
    bar.setBits(0b0000000011);
  } else if (percentage >= 20 && percentage < 30) {
    bar.setBits(0b0000000111);
  } else if (percentage >= 30 && percentage < 40) {
    bar.setBits(0b0000001111);
  } else if (percentage >= 40 && percentage < 50) {
    bar.setBits(0b0000011111);
  } else if (percentage >= 50 && percentage < 60) {
    bar.setBits(0b0000111111); 
  } else if (percentage >= 60 && percentage < 70) {
    bar.setBits(0b0001111111);
  } else if (percentage >= 70 && percentage < 80) {
    bar.setBits(0b0011111111);  
  } else if (percentage >= 80 && percentage < 90) {
    bar.setBits(0b0111111111);  
  } else if (percentage >= 90 && percentage < 100) {
    bar.setBits(0b1111111111); 
  } else {
    bar.setBits(0b0000000000);
  }
}

int getWaterLevel() {
  // 這裡可替換成您的水位感測器讀值
  // 暫用亂數模擬
  return random(0, 101);
}

//--------------------------------------------------------------------------------------
//  以下為新增：發送資料到 ThingSpeak
//--------------------------------------------------------------------------------------
void sendDataToThingSpeak() {
    if (client.connect(server, 80)) {
        Serial.println("Connected to ThingSpeak server!");
        Serial.print("Feeder Food Level: ");
        Serial.println(feederLevelData);
        
        // 將 feederLevelData 上傳到 field1
        String getStr = "GET /update?api_key=" + writeAPIKey +
                        "&field1=" + String(feederLevelData) +
                        " HTTP/1.1\r\n" + 
                        "Host: api.thingspeak.com\r\n" +
                        "Connection: close\r\n\r\n";
        Serial.print(getStr);

        client.print(getStr);
        delay(1000);

        while (client.available()) {
            char c = client.read();
            Serial.write(c);
        }
        client.stop();
        Serial.println("Data sent to ThingSpeak.");
    } else {
        Serial.println("Failed to connect to ThingSpeak.");
    }
}

void printWifiStatus() {
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);

    long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
}
