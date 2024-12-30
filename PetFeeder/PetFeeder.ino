#include <Ultrasonic.h>
#include <math.h>
#include <Grove_LED_Bar.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h> 
#include <Servo.h>

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
unsigned long waterCheckStart = 0;          // 偵測開始時間
const unsigned long waterCheckInterval = 120000; // 2 分鐘偵測週期
int lastWaterLevel = -1;                    // 上次偵測到的水位（可依實際模組做變數調整）
bool waterChanged = false;                  // 水位是否發生變化

//---------------前置設定---------------
void setup() {
    Serial.begin(9600);
    bar.begin();
    lcd.begin(16, 2);
    lcd.backlight(); 
    lcd.setCursor(0,0);
    lcd.print("Awaiting Input...");
    myservo.attach(11);
}

//---------------主迴圈 (僅呼叫函式)---------------
void loop() {
    detectMealTime();
    motorControl();
    detectFoodLevelWithUltrasonic();
    waterStatus();
}

//--------------------------------------------------------------------------------------
//  函式區
//--------------------------------------------------------------------------------------

/**
 * Detect Meal Time：
 * 1) 判斷光感測器讀值與前一讀值之差異 (大於 threshold 代表有吃光)
 * 2) 如果有明顯差異，顯示「Pet has eaten!」，並啟動計時在 LCD 顯示 5 秒
 */
void detectMealTime() {
    int currentReading = analogRead(LIGHT_SENSOR_PIN);
    int lightReadingDiff = abs(currentReading - previousReading);

    // 檢查是否需要顯示「Pet has eaten!」訊息
    if (lightReadingDiff > threshold && !displayMessage) {
      displayMessage = true;
      messageStartTime = millis(); // 記錄當前時間
      lcd.setCursor(0, 0); 
      lcd.print("Pet has eaten!   "); // 確保清除剩餘字元
      Serial.print("Current Light Sensor Reading: ");
      Serial.println(currentReading);
      Serial.print("Light Reading Difference: ");
      Serial.println(lightReadingDiff);
    }

    // 如果訊息正在顯示，檢查是否已達到顯示持續時間
    if (displayMessage) {
      if (millis() - messageStartTime >= displayDuration) {
        lcd.setCursor(0, 0); 
        lcd.print("                "); // 清除LCD前16字元
        displayMessage = false;
      }
    }
    
    previousReading = currentReading; // 更新前一次讀值
}

/**
 * Motor Control：
 * 1) 每隔 feedInterval（此處設定 1 分鐘）檢查是否該投餵
 * 2) 若光感測器偵測到光(表示碗內空)，則打開伺服馬達投餵 0.2 秒
 * 3) 投餵後等待光感測器再次「偵測不到光」才復歸
 *    (此處示範使用簡易的方式，亦可加旗標做更嚴謹的邏輯控制)
 */
void motorControl() {
    unsigned long currentMillis = millis();

    // (1) 判斷是否到了投餵時間
    if ((currentMillis - previousFeedMillis) >= feedInterval) {
        // 到達投餵時間
        previousFeedMillis = currentMillis;
        
        // 先確認「碗內是否空了(光感測器是否讀到光)」
        int currentLight = analogRead(LIGHT_SENSOR_PIN);
        // 閾值可依照實際調整，這裡簡化判定：若越大表示越亮
        if (currentLight > 800) { // 假設 > 500 表示碗空
          Serial.println("== Feeding Time ==");
          isFeeding = true;
          
          // (2) 開啟伺服馬達投餵 (示範為轉到 90 度等 0.2 秒)
//          myservo.write(90);    
          for (pos = 0; pos <= 90; pos += 1) {
             lcd.setCursor(0, 1); 
             lcd.print("Feeding..."); 
             myservo.write(pos);
//              delay(15);
          }
          lcd.print("                "); // 清除LCD前16字元

        }
    }

    // (3) 若目前正在投餵，等待偵測不到光再復歸
    if (isFeeding) {
        int currentLight = analogRead(LIGHT_SENSOR_PIN);
        if (currentLight <= 800) { // 假設 < 300 表示碗內又有飼料、感測到較暗
          // 關閉匝門 (轉回 0 度)
//          myservo.write(0);
          for (pos = 90; pos >= 0; pos -= 1) {
              myservo.write(pos);
//              delay(15);
              lcd.setCursor(0, 1); 
              lcd.print("End Feeding..."); 
          }
          lcd.print("                "); // 清除LCD前16字元
          isFeeding = false;
        }
    }
    
    // 以下為原本示範的馬達 0->180->0 的掃描程式，
    // 您可自行保留或刪除，看是否要使用 sweep 效果。
    /*
    for (pos = 0; pos <= 180; pos += 1) {
      myservo.write(pos);
      delay(15);
    }
    for (pos = 180; pos >= 0; pos -= 1) {
      myservo.write(pos);
      delay(15);
    }
    */
}

/**
 * Detect Food Level With Ultrasonic：
 * 1) 偵測距離轉成剩餘百分比
 * 2) 呼叫 LED Bar 函式顯示
 * 3) 若 < 20% 則顯示「Refill food needed」
 */
void detectFoodLevelWithUltrasonic() {
    long distance = ultrasonic.MeasureInCentimeters();
    // distance 為 0 時有時是非預期情況，可加以排除
    if (distance > 0 && distance <= MAX_HEIGHT) {
      int remainingLevel = MAX_HEIGHT - distance; 
      int percentage = (remainingLevel * 100) / MAX_HEIGHT;
      ledBarHandler(percentage);
      
      // 如果飼料剩餘量低於 20%
      if (percentage < 20) {
         lcd.setCursor(0, 0); 
         lcd.print("Refill food needed");
      }
    } 
    else {
      // 超音波量測超出範圍，可選擇要不要顯示
      // Serial.println("Ultrasonic reading out of range.");
    }
}

/**
 * Water Status：
 * 1) 每隔 waterCheckInterval(2 分鐘) 檢查一次水位(此範例僅示範)
 * 2) 若有水位變化則紀錄
 *    (實際上可依您使用的水位感測器實作對應的讀值與判斷方式)
 */
void waterStatus() {
    unsigned long currentMillis = millis();

    // 初始化: 若尚未設定水位檢測開始時間，則設定
    if (waterCheckStart == 0) {
      waterCheckStart = currentMillis;
    }

    // 每 2 分鐘檢測一次
    if ((currentMillis - waterCheckStart) >= waterCheckInterval) {
      waterCheckStart = currentMillis; 
      int currentWaterLevel = getWaterLevel(); // 假設有個函式讀水位感測器
      // 判斷水位是否變化
      if (lastWaterLevel == -1) {
         lastWaterLevel = currentWaterLevel;
      } else {
         if (abs(currentWaterLevel - lastWaterLevel) > 5) { // 自訂誤差範圍
            // 發生水位變化
            waterChanged = true;
            // 做一些紀錄(此處僅示範輸出)
            Serial.println("Water level changed, record the drinking time!");
         }
         lastWaterLevel = currentWaterLevel;
      }
    }
}

//--------------------------------------------------------------------------------------
//  LED Bar 亮度顯示函式 (保留原本程式)
//--------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------
//  範例取得水位函式 (僅做示範)
//--------------------------------------------------------------------------------------
int getWaterLevel() {
  // 這裡可替換成您的水位感測讀值，例如類比值、數位值等等
  // 先用隨機亂數模擬 (0~100)
  return random(0, 101);
}
