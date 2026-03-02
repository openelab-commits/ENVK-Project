#include <bsec2.h>
#include "M5CoreInk.h"
#include <esp_task_wdt.h>

Ink_Sprite InkPageSprite(&M5.M5Ink);  // 创建墨水屏显示 
Bsec2 ENVSensor;

float currentTemp = 0;
float currentHumidity = 0; 
bool needUpdate = false;
bool ptcState = false;

float setTemperature = 25;
const float TEMP_HYSTERESIS = 0.5;  
const float TEMP_STEP = 5;   // 设定温度与温度回差温度参数定义
const int SW_UP_PIN = 37;     // 滑轮向上引脚
const int SW_DOWN_PIN = 39;   // 滑轮向下引脚
const int PTC_PIN = 26;//控制引脚定义


bool lastUpState = false;
bool lastDownState = false;// 滑轮状态记录      


unsigned long lastSleepTime = 0;
const unsigned long SLEEP_INTERVAL = 6 * 60 * 60 * 1000; // 定时休眠参数 6小时（毫秒）
unsigned long lastTempCheckTime = 0;
const unsigned long TEMP_CHECK_INTERVAL = 5000; // 温度控制检查

float batteryVoltage = 0.0;
int batteryPercentage = 0;//电池显示参数

const int BAT_ADC_PIN = 36;
const float MAX_BATTERY_VOLTAGE = 4.2;
const float MIN_BATTERY_VOLTAGE = 3.0;//定义电池电压引脚

void measureBattery()
{
    int adcValue = analogRead(BAT_ADC_PIN);

    //ADC转电量百分比
    Serial.print("BAT ADC: ");
    Serial.println(adcValue);//ADC串口输出

if (adcValue < 50)
{
    batteryPercentage = 0;
} 
else if (adcValue < 300) 
{ 
    batteryPercentage = map(adcValue, 50, 300, 1, 10);
} 
else if (adcValue < 1000)
{
    batteryPercentage = map(adcValue, 300, 1000, 10, 50);
} 
else if (adcValue < 2000) 
{
    batteryPercentage = map(adcValue, 1000, 2000, 50, 90);
} 
else 
{
     batteryPercentage = map(adcValue, 2000, 2500, 90, 100);
} //分别对应电量显示
    
    batteryPercentage = constrain(batteryPercentage, 0, 100);

    if (adcValue > 100) {
        batteryPercentage = max(batteryPercentage, 1);
    }
    
    Serial.print("Battery: ");
    Serial.print(batteryPercentage);
    Serial.println("%");//电量串口输出
   
}

void updateDisplay()
{
    char tempStr[10];
    char humStr[10];
    char setTempStr[10];
    char batStr[10];

    measureBattery();//测量电池电量

    unsigned long awakeTime = millis() - lastSleepTime;
    InkPageSprite.clear();
    if (awakeTime >= SLEEP_INTERVAL) {
        InkPageSprite.drawString(60, 6, "SLEEP");
    } else if (awakeTime >= SLEEP_INTERVAL - 60000) {
        InkPageSprite.drawString(60, 6, "....");
    } else {
        InkPageSprite.drawString(60, 6, "AWAKE");
    }//休眠状态显示

    sprintf(batStr, "%d%%", batteryPercentage);
    InkPageSprite.drawString(140, 6, batStr);  // 显示电量百分比
    
    // pinMode(36,INPUT_PULLUP);
    // bool isCharging = (digitalRead(36) == LOW);

    // if (isCharging) {
        
    //     InkPageSprite.drawString(160, 6, " + "); //充电显示""+"
    // } 

    // Serial.print("GPIO36: ");
    // Serial.println(digitalRead(36));
    // Serial.print("IsCharging");
    // Serial.println(isCharging);//串口输出调试
             
    
    sprintf(tempStr, "%.1f", currentTemp);
    sprintf(humStr, "%.1f", currentHumidity); 
    sprintf(setTempStr, "%.1f", setTemperature);  

    InkPageSprite.drawString(20, 140, "PTC: ");  
    if (ptcState) 
    {
        InkPageSprite.drawString(100, 140, "ON");  
    } else {
        InkPageSprite.drawString(100, 140, "OFF");  
    }// 显示PTC状态
   
    InkPageSprite.drawString(20, 20, "SET TEMP: "); 
    InkPageSprite.drawString(80, 40, setTempStr);  
    InkPageSprite.drawString(160, 40, "C");//显示设定温度

    InkPageSprite.drawString(20, 60, "TEMP"); 
    InkPageSprite.drawString(50, 80, tempStr);  
    InkPageSprite.drawString(140, 80, "C");//显示实时温度相关信息
    InkPageSprite.drawString(20, 100, "HUMD");  
    InkPageSprite.drawString(50, 120, humStr);  
    InkPageSprite.drawString(140, 120, "%"); // 显示实时湿度相关信息

    InkPageSprite.drawString(20, 160, "ENV Pro + Core Ink"); // 底部其相关信息
    InkPageSprite.pushSprite();// 更新到屏幕

}//屏幕更新函数

void readTemperatureSetting()
{
    bool currentUpState = digitalRead(SW_UP_PIN);       
    bool currentDownState = digitalRead(SW_DOWN_PIN);
    
    
        if (currentUpState == LOW && lastUpState == HIGH) { 
            setTemperature += TEMP_STEP;
            needUpdate = true;
            Serial.println("updata  to: " + String(setTemperature));
            
        } // 向上滑动,增加温度

        if (currentDownState == LOW && lastDownState == HIGH) { 
            setTemperature -= TEMP_STEP;
            needUpdate = true;
            Serial.println("declare to: " + String(setTemperature));

        }// 向下滑动,降低温度
        
        lastUpState = currentUpState;
        lastDownState = currentDownState;
    
    
}//滑动电位器温度读取模块

void controlPTC()
{
     if (currentTemp < (setTemperature - TEMP_HYSTERESIS)) {
        if (!ptcState) {
            digitalWrite(PTC_PIN, HIGH);  // 开启PTC
            ptcState = true;
            Serial.println("PTC OPEN");
            needUpdate = true;
        }
    }
    else if (currentTemp > (setTemperature + TEMP_HYSTERESIS)) {
        if (ptcState) {
            digitalWrite(PTC_PIN, LOW);  // 关闭PTC
            ptcState = false;
            Serial.println("PTC CLOSE");
            needUpdate = true;
        }
    }

}//PTC控制模块

void newDataCallback(const bme68xData data,const bsecOutputs outputs,Bsec2 bsec)
{
    if (!outputs.nOutputs) return;

    Serial.println(
        "BSEC outputs:\n\ttimestamp = " +
        String((int)(outputs.output[0].time_stamp / INT64_C(1000000))));
    
    for (uint8_t i = 0; i < outputs.nOutputs; i++) {
        const bsecData output = outputs.output[i];
        switch (output.sensor_id) {
            case BSEC_OUTPUT_RAW_TEMPERATURE:
                Serial.println("\ttemperature = " + String(output.signal));
                currentTemp = output.signal;  // 保存温度值
                needUpdate = true;  // 设置更新标志
                break;
            case BSEC_OUTPUT_RAW_HUMIDITY:  
                Serial.println("\thumidity = " + String(output.signal));
                currentHumidity = output.signal; // 保存湿度值
                needUpdate = true; //更新
                break;
            default:
                break;
        }
    }   
}

void checkBsecStatus(Bsec2 bsec)
{
if (bsec.status < BSEC_OK) {
        Serial.println("BSEC error code : " + String(bsec.status));

    } else if (bsec.status > BSEC_OK) {
        Serial.println("BSEC warning code : " + String(bsec.status));
    }

    if (bsec.sensor.status < BME68X_OK) {
        Serial.println("BME68X error code : " + String(bsec.sensor.status));
    } else if (bsec.sensor.status > BME68X_OK) {
        Serial.println("BME68X warning code : " + String(bsec.sensor.status));
    }
}

void setup(void)
{
   
    esp_task_wdt_init(30, true);  // 30秒看门狗
    esp_task_wdt_add(NULL);        // 添加当前任务

    Serial.begin(115200);
    delay(100);
    Serial.println("Creating..."); 
    delay(2000);
    Serial.println("Connecting Screen... "); 
    delay(2000);

    M5.begin(true, true, false);  // 连接CoreInk墨水屏,I2C通讯；禁用扬声器
    
   
    Serial.end();                 
    delay(100);                  
    Serial.begin(115200);         
    delay(100);            //由于M5.begin又重新初始化串口，导致串口打印无法正常输出，故再次关闭串口再重新初始化
    
    Serial.println("Connected successfully");  

    if (!M5.M5Ink.isInit())
        { 
        Serial.printf("Ink Init faild");
        }                     // 检查初始化是否成功
        M5.M5Ink.clear();  
        delay(1000);// 清屏

    if (InkPageSprite.creatSprite(0, 0, 200, 200, true) != 0) 
    {
        Serial.printf("Ink Sprite create faild");
    }
    InkPageSprite.drawString(20, 20, "Hello Core-INK");//创建图像区域

    pinMode(PTC_PIN,OUTPUT);
    digitalWrite(PTC_PIN,LOW);//PTC控制引脚初始化
    
    pinMode(SW_UP_PIN, INPUT_PULLUP);   
    pinMode(SW_DOWN_PIN, INPUT_PULLUP);//电位器引脚初始化

    pinMode(BAT_ADC_PIN, INPUT); 
    analogReadResolution(12);     
    analogSetAttenuation(ADC_11db); //电池电压检测引脚初始化
    

    if(!ENVSensor.begin(BME68X_I2C_ADDR_HIGH,Wire))
    {
        checkBsecStatus(ENVSensor);
    }//初始化传感器 

    bsecSensor sensorList[]={BSEC_OUTPUT_RAW_TEMPERATURE,BSEC_OUTPUT_RAW_HUMIDITY};
    if (!ENVSensor.updateSubscription(sensorList, ARRAY_LEN(sensorList),BSEC_SAMPLE_RATE_LP)) 
    {
        checkBsecStatus(ENVSensor);
    }//订阅BSEC2输出数据

    ENVSensor.attachCallback(newDataCallback);
    Serial.println("BSEC library version " + String(ENVSensor.version.major) +
                   "." + String(ENVSensor.version.minor) + "." +
                   String(ENVSensor.version.major_bugfix) + "." +
                   String(ENVSensor.version.minor_bugfix));
    //调用函数

    InkPageSprite.clear();
    InkPageSprite.drawString(70, 80, "ENV Pro");
    InkPageSprite.drawString(20, 110, "TEMPERATURE+HUMIDITY");
    InkPageSprite.pushSprite();

    delay(2000);//显示

    readTemperatureSetting();//设定温度初始化读取
    updateDisplay();
}

void loop(void)
{
    esp_task_wdt_reset();  // 喂狗，防止重启
    M5.update();//实时更新

    if (millis() - lastSleepTime > SLEEP_INTERVAL) 
    {
        Serial.println("sleep mode");
        digitalWrite(PTC_PIN, LOW);//休眠同时关闭PTC
        M5.shutdown(10); 
        lastSleepTime = millis(); 
        return; // 休眠10s
    }


     if (!ENVSensor.run()) {
        checkBsecStatus(ENVSensor);
    }//传感器运行

     readTemperatureSetting();//读取滑轮设定温度

    if (millis() - lastTempCheckTime > TEMP_CHECK_INTERVAL) {
        controlPTC();
        lastTempCheckTime = millis();
    }// 定期检查温度控制

    if (needUpdate) 
    {  
        updateDisplay();// 有新数据时更新显示
        needUpdate = false;  // 重置标志
    }
    delay(1000);//设置延迟
}           
