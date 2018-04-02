/**
   Desenvolvido originalmente por Bruno Horta
   para o BH onofre conforme exemplo aqui: https://www.youtube.com/watch?v=OyY4ymv6db0

   Adaptado para o Aquario de casa por steam228

   original by Bruno Horta,
   adapted for an hot water fish tank by steam228

 * */


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
#define aSensor A0



//CONSTANTS
const String HOSTNAME  = "fishtankLOU";
const char * OTA_PASSWORD  = "herbertolevah";
const String MQTT_LOG = "system/log";
const String MQTT_SYSTEM_CONTROL_TOPIC = "system/set/"+HOSTNAME;
const String MQTT_LIGHT_ONE_TOPIC = "relay/one/set";
const String MQTT_LIGHT_TWO_TOPIC = "relay/two/set";
const String MQTT_LIGHT_ONE_STATE_TOPIC = "relay/one";
const String MQTT_LIGHT_TWO_STATE_TOPIC = "relay/two";
const String MQTT_ANALOG_SENSOR_PUBLISH_TOPIC = "sensor/distance/value";
//MQTT BROKERS GRATUITOS PARA TESTES https://github.com/mqtt/mqtt.github.io/wiki/public_brokers
const char* MQTT_SERVER = "192.168.1.127";

WiFiClient wclient;
PubSubClient client(MQTT_SERVER,1883,wclient);

//CONTROL FLAGS
bool OTA = false;
bool OTABegin = false;
bool lastButtonState = false;

//otherVariables

float analogSensorValue = 0;
long lastPub = 0;
char SensorValue[50];


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
  pinMode(TOUCH,INPUT_PULLUP);
}
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

   analogSensorValue = analogRead(aSensor);
   analogSensorValue = map(analogSensorValue,0,1023,0,400);
   String SensorValue = String(analogSensorValue,1);


  if (WiFi.status() == WL_CONNECTED) {
    if (checkMqttConnection()){
      if (millis()-lastPub >= 5000){
        Serial.println(SensorValue);
        client.publish(MQTT_ANALOG_SENSOR_PUBLISH_TOPIC.c_str(), SensorValue.c_str());
        lastPub = millis();
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
