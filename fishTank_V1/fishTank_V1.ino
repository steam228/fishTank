/**
   Desenvolvido originalmente por Bruno Horta
   para o BH onofre conforme exemplo aqui: https://www.youtube.com/watch?v=OyY4ymv6db0
   https://github.com/brunohorta82/BH_OnOfre
   
   Adaptado para o Aquario de casa por steam228

   original by Bruno Horta,
   adapted for an hot water fish tank by steam228

 * */
#include <Timing.h>

#include <PubSubClient.h>
//ESP
#include <ESP8266WiFi.h>'
//Wi-Fi Manager library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>//https://github.com/tzapu/WiFiManager
//OTA
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
//Other Libs
#include <OneWire.h>
#include <DallasTemperature.h>
//#include <Servo.h> //para fishFeeder

            
#define AP_TIMEOUT 180
#define SERIAL_BAUDRATE 115200

//MQTT
#define MQTT_AUTH false
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""

//PORTS
#define RELAY_ONE 5
#define RELAY_TWO 4
#define TOUCH 13

//pH
#define pHsensor A0

//TEMPERATURA
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);

//LEDS
#define LEDph 14 //Led pH alerta
#define LEDtemp 12 //led Temperatura Alerta


Timing mytimer;

//CONSTANTS
const String HOSTNAME  = "fishtankLOU";
const char * OTA_PASSWORD  = "herbertolevah";
const String MQTT_LOG = "system/log";
const String MQTT_SYSTEM_CONTROL_TOPIC = "system/set/"+HOSTNAME;
const String MQTT_LIGHT_ONE_TOPIC = "relay/one/set";
const String MQTT_LIGHT_TWO_TOPIC = "relay/two/set";
const String MQTT_LIGHT_ONE_STATE_TOPIC = "relay/one";
const String MQTT_LIGHT_TWO_STATE_TOPIC = "relay/two";
//sensors
const String MQTT_WATER_pH_PUBLISH_TOPIC = "sensor/ph";
const String MQTT_WATER_temp_PUBLISH_TOPIC = "sensor/temp";


//MQTT BROKERS GRATUITOS PARA TESTES https://github.com/mqtt/mqtt.github.io/wiki/public_brokers
const char* MQTT_SERVER = "192.168.1.127";

WiFiClient wclient;
PubSubClient client(MQTT_SERVER,1883,wclient);

//CONTROL FLAGS
bool OTA = false;
bool OTABegin = false;
bool lastButtonState = false;

//otherVariables

float temp;
float pH;
float filterVal = .0001;       // this determines smoothness  - .0001 is max  1 is off (no smoothing)



void setup() {
  Serial.begin(SERIAL_BAUDRATE);
  WiFiManager wifiManager;
  //reset saved settings
  //wifiManager.resetSettings();
  /*define o tempo limite até o portal de configuração ficar novamente inátivo,
  útil para quando alteramos a password do AP*/
  wifiManager.setTimeout(AP_TIMEOUT);
  if(!wifiManager.autoConnect(HOSTNAME.c_str())) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    ESP.restart();
    delay(5000);
  }
  client.setCallback(callback);
  pinMode(RELAY_ONE,OUTPUT);
  pinMode(RELAY_TWO,OUTPUT);
  pinMode(LEDph,OUTPUT);
  pinMode(LEDtemp,OUTPUT);
  pinMode(TOUCH,INPUT_PULLUP);

  digitalWrite(LEDph,LOW);
  digitalWrite(LEDtemp,LOW);
  mytimer.begin(0);
}

//RELAY
void turnOn1(){
  digitalWrite(RELAY_ONE,HIGH);
  client.publish(MQTT_LIGHT_ONE_STATE_TOPIC.c_str(),"ON");
}

void turnOff1(){
  digitalWrite(RELAY_ONE,LOW);
  client.publish(MQTT_LIGHT_ONE_STATE_TOPIC.c_str(),"OFF");
}

void turnOn2(){
  digitalWrite(RELAY_TWO,HIGH);
  client.publish(MQTT_LIGHT_TWO_STATE_TOPIC.c_str(),"ON");
}

void turnOff2(){
  digitalWrite(RELAY_TWO,LOW);
  client.publish(MQTT_LIGHT_TWO_STATE_TOPIC.c_str(),"OFF");
}


void publica(){

   float sensorValue = analogRead(A0);
    sensorValue = 0.011235955*sensorValue-2.101123596; //https://hackaday.io/project/25714-connected-pool-monitor/log/63762-measuring-ph-and-orp-with-arduino
    pH =  smooth(sensorValue, filterVal, pH);
    String pHvalue = String(pH,1);
    DS18B20.requestTemperatures();
    temp = DS18B20.getTempCByIndex(0);
    String waterTemp = String(temp,1);
    
   
   alerta();
   Serial.println(pH);
   client.publish(MQTT_WATER_pH_PUBLISH_TOPIC.c_str(), pHvalue.c_str());
   Serial.println(temp);
   client.publish(MQTT_WATER_temp_PUBLISH_TOPIC.c_str(), waterTemp.c_str()); 
 }


//Chamada de recepção de mensagem
void callback(char* topic, byte* payload, unsigned int length) {
  String payloadStr = "";
  for (int i=0; i<length; i++) {
    payloadStr += (char)payload[i];
  }
  Serial.println(payloadStr);
  String topicStr = String(topic);
  if(topicStr.equals(MQTT_SYSTEM_CONTROL_TOPIC)){
    if(payloadStr.equals("OTA_ON")){
      OTA = true;
      OTABegin = true;
    }else if (payloadStr.equals("OTA_OFF")){
      OTA = true;
      OTABegin = true;
    }else if (payloadStr.equals("REBOOT")){
      ESP.restart();
    }
  } else if(topicStr.equals(MQTT_LIGHT_ONE_TOPIC)){
    Serial.println(payloadStr);
    if(payloadStr.equals("ON")){

      turnOn1();

    }else if(payloadStr.equals("OFF")) {

      turnOff1();

    }

  }else if(topicStr.equals(MQTT_LIGHT_TWO_TOPIC)){
    Serial.println(payloadStr);
    if(payloadStr.equals("ON")){

      turnOn2();

    }else if(payloadStr.equals("OFF")) {

      turnOff2();

    }

  }
}
void handleInterrupt() {
  if(digitalRead(RELAY_ONE)){
    turnOff1();
  }else{
    turnOn1();
  }
}
bool checkMqttConnection(){
  if (!client.connected()) {
    if (MQTT_AUTH ? client.connect(HOSTNAME.c_str(),MQTT_USERNAME, MQTT_PASSWORD) : client.connect(HOSTNAME.c_str())) {
      //SUBSCRIÇÃO DE TOPICOS
      Serial.println("CONNECTED");
      client.subscribe(MQTT_SYSTEM_CONTROL_TOPIC.c_str());
      client.subscribe(MQTT_LIGHT_ONE_TOPIC.c_str());
      client.subscribe(MQTT_LIGHT_TWO_TOPIC.c_str());

      client.publish(MQTT_LOG.c_str(),"Hello Aquario do Tomás e do Manuel");
    }
  }

  return client.connected();
}

void loop() {

   


  if (WiFi.status() == WL_CONNECTED) {
    if (checkMqttConnection()){
      if (mytimer.onTimeout(5000)){
        publica();
        } 
      bool realState = digitalRead(TOUCH);
      if(lastButtonState != realState){
        handleInterrupt();
        lastButtonState = realState;
      }
      client.loop();
      if(OTA){
        if(OTABegin){
          setupOTA();
          OTABegin= false;
        }
        ArduinoOTA.handle();
      }
    }
  }
}

void setupOTA(){
  if (WiFi.status() == WL_CONNECTED && checkMqttConnection()) {
    client.publish(MQTT_LOG.c_str(),"OTA SETUP ON");
    ArduinoOTA.setHostname(HOSTNAME.c_str());
    ArduinoOTA.setPassword((const char *)OTA_PASSWORD);

    ArduinoOTA.onStart([]() {
      client.publish(MQTT_LOG.c_str(),"START");
    });
    ArduinoOTA.onEnd([]() {
      client.publish(MQTT_LOG.c_str(),"END");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      String p = "Progress: "+ String( (progress / (total / 100)));
      client.publish(MQTT_LOG.c_str(),p.c_str());
    });
    ArduinoOTA.onError([](ota_error_t error) {
      if (error == OTA_AUTH_ERROR) client.publish(MQTT_LOG.c_str(),"Auth Failed");
      else if (error == OTA_BEGIN_ERROR)client.publish(MQTT_LOG.c_str(),"Auth Failed");
      else if (error == OTA_CONNECT_ERROR)client.publish(MQTT_LOG.c_str(),"Connect Failed");
      else if (error == OTA_RECEIVE_ERROR)client.publish(MQTT_LOG.c_str(),"Receive Failed");
      else if (error == OTA_END_ERROR)client.publish(MQTT_LOG.c_str(),"End Failed");
    });
    ArduinoOTA.begin();
  }
}

void alerta(){
  
  if(pH <6 || pH > 8){  
    digitalWrite(LEDph,HIGH);
    }
  else{
    digitalWrite(LEDph,LOW);
    }
   if(temp < 25 || temp > 28.5 ){  
    digitalWrite(LEDtemp,HIGH);
    }
   else{
    digitalWrite(LEDtemp,LOW);
    }     
   
  }

float smooth(float data, float filterVal, float smoothedVal) {


  if (filterVal > 1) {     // check to make sure param's are within range
    filterVal = .99;
  }
  else if (filterVal <= 0) {
    filterVal = 0;
  }

  smoothedVal = (data * (1 - filterVal)) + (smoothedVal  *  filterVal);

  return (float)smoothedVal;
}
