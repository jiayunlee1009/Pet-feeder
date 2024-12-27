#include <Ultrasonic.h>
#include <math.h>
#include <Grove_LED_Bar.h>

#define LIGHT_SENSOR_PIN A0
Ultrasonic ultrasonic(2);
Grove_LED_Bar bar(5, 4, 0);

const int MAX_HEIGHT = 30; // 飼料槽總高度（公分）待設定
const int threshold = 500; //光感差閾值待設定
int previousReading = 0;

void setup() {
  Serial.begin(9600);
  bar.begin();
}

void loop() {
  int currentReading = analogRead(LIGHT_SENSOR_PIN);
  if (abs(currentReading - previousReading) > threshold) {
    //abs() 是一個取絕對值的函數，確保變化量為正值
    //利用與前一次的光感測數據比較可避免環境光變化造成的誤判
    Serial.println("Pet has eaten!");
  }else {
    Serial.println("Pet has not eaten yet!");
  }
  previousReading = currentReading;

  long distance = ultrasonic.MeasureInCentimeters();
  if (distance > 0 && distance <= MAX_HEIGHT) {
    int remainingLevel = MAX_HEIGHT - distance; 
    int percentage = (remainingLevel * 100) / MAX_HEIGHT;
    Serial.print("Remaining feed level: ");
    Serial.print(percentage);
    Serial.println(" %");
    ledBarHandler(percentage);

    if (percentage < 20) {
      Serial.println("Low feed level! Please refill.");
    }
  } else {
    Serial.println("Ultrasonic reading out of range.");
  }
  // 3. 延遲
//  delay(1000); // 每秒更新一次
}

void ledBarHandler(int percentage) {
  if (percentage < 10) {
    Serial.print("LED: Lower than 10%");
    bar.setBits(0b1111111111);
  } else if (percentage >= 10 && percentage < 20) {
    Serial.print("LED: Lower than 10%");
    bar.setBits(0b0111111111);
  } else if (percentage >= 20 && percentage < 30) {
    Serial.print("LED: between 10 - 20%");
    bar.setBits(0b0011111111);
  } else if (percentage >= 30 && percentage < 40) {
    Serial.print("LED: between 20 - 30%");
    bar.setBits(0b0001111111);
  } else if (percentage >= 40 && percentage < 50) {
    Serial.print("LED: between 40 - 50%");
    bar.setBits(0b0000111111);
  } else if (percentage >= 50 && percentage < 60) {
    Serial.print("LED: between 50 - 60%");
    bar.setBits(0b0000011111); 
  } else if (percentage >= 60 && percentage < 70) {
    Serial.print("LED: between 60 - 70%");
    bar.setBits(0b0000001111);
  } else if (percentage >= 70 && percentage < 80) {
    Serial.print("LED: between 70 - 80%");
    bar.setBits(0b0000000111);  
  } else if (percentage >= 80 && percentage < 90) {
    Serial.print("LED: between 80 - 90%");
    bar.setBits(0b0000000011);  
  } else if (percentage >= 90 && percentage < 100) {
    Serial.print("LED: more than 90%");
    bar.setBits(0b0000000001); 
  } else {
    Serial.print("LED closed");
    bar.setBits(0b0000000000);
  }
}
