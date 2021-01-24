#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

#define DEVICELENGTH 4 // maximum 4 devices can be added in ESP8266

const char* ssid = "SSID";                                        // wifi name
const char* password = "PASSWORD";                                // wifi password
const char* mqtt_server = "MQTT_SERVER";                          // mqtt server name
String device[DEVICELENGTH] = {"esp01","esp02","esp03","esp04"};  // change the device name to your liking but device id must be unique !!
int relays[DEVICELENGTH] = { 10,13,12,14 };                       // GPIO pins to be connected in the relay
int switches[DEVICELENGTH] = { 9,4,5,3 };                         // Switch inputs
int tempState[DEVICELENGTH];
int previousState[DEVICELENGTH];
int i;
long lastReconnectAttempt = 0;

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  EEPROM.begin(4);
  Serial.println("SETUP STARTS");
  for (i = 0; i < DEVICELENGTH; i++) {
    pinMode(relays[i], OUTPUT);          // Declaring relay pins as output
    pinMode(switches[i], INPUT_PULLUP);  //Declaring switch pins as input (INPUT_PULLUP: For eliminating AC interference)
    digitalWrite(relays[i], 1);          //Set relay OFF initially (0=ON, 1=OFF)
    tempState[i] = EEPROM.read(i);
    Serial.println("SETUP STARTS:" + tempState[i] );
    wifiLogic(tempState[i],relays[i],i);
    }

  Serial.println("Booting");
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  lastReconnectAttempt = 0;             //mqtt code
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  ArduinoOTA.handle();
  if (!client.connected()) {
    for(i = 0; i < DEVICELENGTH; i++)
       tempState[i] = switchStates(switches[i],relays[i],i,tempState[i]);
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    // Client connected
   //To operate relay using physical switch
  for(i = 0; i < DEVICELENGTH; i++)
       tempState[i] = switchStates(switches[i],relays[i],i,tempState[i]);
       
//For publishing the state of relay using MQTT 
  for(i = 0; i < DEVICELENGTH; i++)
  previousState[i] = publishRelayChange(device[i],relays[i],previousState[i],i);
   ArduinoOTA.handle();
    client.loop();
  }

}

//Subscribe to topics when connected to MQTT
void subscription(String deviceId,int relay)
{
  String OnOffTopic = "device/" + deviceId + "/OnOff";
  String currentStateTopic = "device/" + deviceId + "/currentstate";

  client.subscribe((char*) OnOffTopic.c_str());
  client.subscribe((char*) currentStateTopic.c_str());
  currentState(deviceId,relay);
}

boolean reconnect() {
  String clientId = "ESP8266Client-";
  clientId += String(random(0xffff), HEX);
  if (client.connect(clientId.c_str())) {
    // Once connected, publish an announcement...
    Serial.println("MQTT connected");
    for(i = 0; i < DEVICELENGTH; i++)
       subscription(device[i],relays[i]);

  }
  return client.connected();
}




void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
  char message = payload[0];                        //convert byte to char
  Serial.print("Topic from MQTT broker is : ");
  Serial.print(topic);
  Serial.print(" ");
  Serial.println(message);
  for(i = 0; i < DEVICELENGTH; i++) {
   //if(strstr(topic,(char*) device[i].c_str()) != NULL)
     tempState[i] = compareTopic(topic,message,device[i],relays[i],i);
} 
}
// Compare the topics form MQTT
int compareTopic(String topic,char message, String deviceId,int relay,int addr)
{
  String OnOffTopic = "device/" + deviceId + "/OnOff";
  String currentStateTopic = "device/" + deviceId + "/currentstate";
  
  if (strcmp(topic.c_str(),(char*) currentStateTopic.c_str()) == 0)
      currentState(deviceId,relay);
  if (strcmp(topic.c_str(), (char*) OnOffTopic.c_str()) == 0)
    {
      if(message == '1')
          wifiLogic(true,relay,addr);  //Returns tempstate
      if(message == '0')
          wifiLogic(false,relay,addr); //Returns tempstate
    }
}


//To operate relay using WiFi
void wifiLogic(bool wifiState,int relay,int addr)
{
  Serial.println("inside wifiState func");
  Serial.println(wifiState);
  Serial.println(digitalRead(relay));
  if(wifiState == true && digitalRead(relay))
    {
      digitalWrite(relay, 0);  //Relay on
      return;
    }
  else if(wifiState == false && !digitalRead(relay))
    {
      digitalWrite(relay, 1);  //Relay off
      return;
    }
   else
    return;
}

//For publishing the current state of relay through MQTT 
void currentState(String deviceId,int relay) 
{ 
      String publishtTopic = "device/" + deviceId + "/state";
      String currentState = digitalRead(relay) == 0 ? "1" : "0";
      client.publish((char*) publishtTopic.c_str(), (char*) currentState.c_str());
      Serial.println("currentState published, deviceId:" + deviceId);
}

//For publishing the state of relay through MQTT 
int publishRelayChange(String deviceId,int relay,int previousState,int i)
{
  int relayState = digitalRead(relay); 
  if(relayState != previousState)
  {
    String currentState = digitalRead(relay) == 0 ? "1" : "0";
    String topic = "device/" + deviceId + "/state";
    client.publish((char*) topic.c_str(), (char*) currentState.c_str());
    Serial.println(currentState + " published, deviceId: " + deviceId + "relay: " + relay);
    digitalRead(relay) == 0 ? EEPROM.write(i, HIGH) : EEPROM.write(i, LOW);
    if (EEPROM.commit())
      Serial.println("EEPROM successfully committed");
    previousState = relayState;
  }  
  return previousState;
}

//To operate relay using physical switch
int switchStates(int button,int relay,int addr,int tempState)
{
   if(digitalRead(button) == LOW)
    {
      if(tempState == HIGH)
      { 
       digitalWrite(relay, 0);           //Relay on
      }
       return LOW;
    }
    if(digitalRead(button) == HIGH )
    {
      if(tempState == LOW)
      {
        digitalWrite(relay, 1);         //Relay off
      }
        return HIGH;
    }
  delay(100);
}
