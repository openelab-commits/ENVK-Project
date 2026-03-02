#include <bsec2.h>
#include "M5CoreInk.h"
#include <esp_task_wdt.h>

Ink_Sprite InkPageSprite(&M5.M5Ink);  // Create E-INK sprite object
Bsec2 ENVSensor;

float currentTemp = 0;
float currentHumidity = 0; 
bool needUpdate = false;
bool ptcState = false;

float setTemperature = 25;
const float TEMP_HYSTERESIS = 0.5;  
const float TEMP_STEP = 5;   // Define temperature parameters: set temperature and hysteresis
const int SW_UP_PIN = 37;     // Scroll wheel up pin
const int SW_DOWN_PIN = 39;   // Scroll wheel down pin
const int PTC_PIN = 26;       // PTC control pin definition


bool lastUpState = false;
bool lastDownState = false;   // Scroll wheel state record      


unsigned long lastSleepTime = 0;
const unsigned long SLEEP_INTERVAL = 6 * 60 * 60 * 1000; // Timed sleep parameter: 6 hours (milliseconds)
unsigned long lastTempCheckTime = 0;
const unsigned long TEMP_CHECK_INTERVAL = 5000; // Temperature control check interval (5s)



void updateDisplay()
{
    char tempStr[10];
    char humStr[10];
    char setTempStr[10];

    unsigned long awakeTime = millis() - lastSleepTime;
    InkPageSprite.clear();
    if (awakeTime >= SLEEP_INTERVAL) {
        InkPageSprite.drawString(80, 6, "SLEEP");
    } else if (awakeTime >= SLEEP_INTERVAL - 60000) {
        InkPageSprite.drawString(80, 6, "....");
    } else {
        InkPageSprite.drawString(80, 6, "AWAKE");
    } // Sleep status display

    sprintf(tempStr, "%.1f", currentTemp);
    sprintf(humStr, "%.1f", currentHumidity); 
    sprintf(setTempStr, "%.1f", setTemperature);  

    InkPageSprite.drawString(20, 140, "PTC: ");  
    if (ptcState) 
    {
        InkPageSprite.drawString(100, 140, "ON");  
    } else {
        InkPageSprite.drawString(100, 140, "OFF");  
    } // Display PTC status
   
    InkPageSprite.drawString(20, 20, "SET TEMP: "); 
    InkPageSprite.drawString(80, 40, setTempStr);  
    InkPageSprite.drawString(160, 40, "C"); // Display set temperature

    InkPageSprite.drawString(20, 60, "TEMP"); 
    InkPageSprite.drawString(50, 80, tempStr);  
    InkPageSprite.drawString(140, 80, "C"); // Display real-time temperature related info
    InkPageSprite.drawString(20, 100, "HUMD");  
    InkPageSprite.drawString(50, 120, humStr);  
    InkPageSprite.drawString(140, 120, "%"); // Display real-time humidity related info

    InkPageSprite.drawString(20, 160, "ENV Pro + Core Ink"); // Bottom related info
    InkPageSprite.pushSprite(); // Update to screen

} // Screen update function

void readTemperatureSetting()
{
    bool currentUpState = digitalRead(SW_UP_PIN);       
    bool currentDownState = digitalRead(SW_DOWN_PIN);
    
    
    if (currentUpState == LOW && lastUpState == HIGH) { 
        setTemperature += TEMP_STEP;
        needUpdate = true;
        Serial.println("update to: " + String(setTemperature));
        
    } // Scroll up to increase temperature

    if (currentDownState == LOW && lastDownState == HIGH) { 
        setTemperature -= TEMP_STEP;
        needUpdate = true;
        Serial.println("decrease to: " + String(setTemperature));

    } // Scroll down to decrease temperature
    
    lastUpState = currentUpState;
    lastDownState = currentDownState;
    
} // Scroll wheel temperature reading module

void controlPTC()
{
    if (currentTemp < (setTemperature - TEMP_HYSTERESIS)) {
        if (!ptcState) {
            digitalWrite(PTC_PIN, HIGH);  // Turn on PTC
            ptcState = true;
            Serial.println("PTC OPEN");
            needUpdate = true;
        }
    }
    else if (currentTemp > (setTemperature + TEMP_HYSTERESIS)) {
        if (ptcState) {
            digitalWrite(PTC_PIN, LOW);  // Turn off PTC
            ptcState = false;
            Serial.println("PTC CLOSE");
            needUpdate = true;
        }
    }

} // PTC control module

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
                currentTemp = output.signal;  // Save temperature value
                needUpdate = true;  // Set update flag
                break;
            case BSEC_OUTPUT_RAW_HUMIDITY:  
                Serial.println("\thumidity = " + String(output.signal));
                currentHumidity = output.signal; // Save humidity value
                needUpdate = true; // Update flag
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
   
    esp_task_wdt_init(30, true);  // 30s watchdog timer initialization
    esp_task_wdt_add(NULL);        // Add current task to watchdog

    Serial.begin(115200);
    delay(100);
    Serial.println("Creating..."); 
    delay(2000);
    Serial.println("Connecting Screen... "); 
    delay(2000);

    M5.begin(true, true, false);  // Initialize CoreInk E-Ink screen, I2C communication; disable speaker
    
   
    Serial.end();                 
    delay(100);                  
    Serial.begin(115200);         
    delay(100);            // M5.begin re-initializes serial port causing output issues, so re-initialize serial port
    
    Serial.println("Connected successfully");  

    if (!M5.M5Ink.isInit())
    { 
        Serial.printf("Ink Init failed");
    }                     // Check if initialization is successful
    M5.M5Ink.clear();  
    delay(1000); // Clear screen

    if (InkPageSprite.creatSprite(0, 0, 200, 200, true) != 0) 
    {
        Serial.printf("Ink Sprite create failed");
    }
    InkPageSprite.drawString(20, 20, "Hello Core-INK"); // Create drawing area

    pinMode(PTC_PIN,OUTPUT);
    digitalWrite(PTC_PIN,LOW); // PTC control pin initialization
    
    pinMode(SW_UP_PIN, INPUT_PULLUP);   
    pinMode(SW_DOWN_PIN, INPUT_PULLUP); // Scroll wheel pin initialization
    

    if(!ENVSensor.begin(BME68X_I2C_ADDR_HIGH,Wire))
    {
        checkBsecStatus(ENVSensor);
    } // Initialize sensor 

    bsecSensor sensorList[]={BSEC_OUTPUT_RAW_TEMPERATURE,BSEC_OUTPUT_RAW_HUMIDITY};
    if (!ENVSensor.updateSubscription(sensorList, ARRAY_LEN(sensorList),BSEC_SAMPLE_RATE_LP)) 
    {
        checkBsecStatus(ENVSensor);
    } // Subscribe to BSEC2 output data

    ENVSensor.attachCallback(newDataCallback);
    Serial.println("BSEC library version " + String(ENVSensor.version.major) +
                   "." + String(ENVSensor.version.minor) + "." +
                   String(ENVSensor.version.major_bugfix) + "." +
                   String(ENVSensor.version.minor_bugfix));
    // Call callback function

    InkPageSprite.clear();
    InkPageSprite.drawString(70, 80, "ENV Pro");
    InkPageSprite.drawString(20, 110, "TEMPERATURE+HUMIDITY");
    InkPageSprite.pushSprite();

    delay(2000); // Display init info

    readTemperatureSetting(); // Initialize temperature setting reading
    updateDisplay();
}

void loop(void)
{
    esp_task_wdt_reset();  // Feed watchdog to prevent reboot
    M5.update(); // Real-time update M5CoreInk status

    if (millis() - lastSleepTime > SLEEP_INTERVAL) 
    {
        Serial.println("sleep mode");
        digitalWrite(PTC_PIN, LOW); // Turn off PTC when entering sleep mode
        M5.shutdown(10); 
        lastSleepTime = millis(); 
        return; // Sleep for 10 seconds
    }


    if (!ENVSensor.run()) {
        checkBsecStatus(ENVSensor);
    } // Run sensor data collection

    readTemperatureSetting(); // Read scroll wheel set temperature

    if (millis() - lastTempCheckTime > TEMP_CHECK_INTERVAL) {
        controlPTC();
        lastTempCheckTime = millis();
    } // Regularly check temperature control

    if (needUpdate) 
    {  
        updateDisplay(); // Update display when new data is available
        needUpdate = false;  // Reset update flag
    }
    delay(1000); // Set delay time
}