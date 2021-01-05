#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <PubSubClient.h>
#include <EEPROM.h>
#define DEVICELENGTH 4 // maximum 4 devices can be added in ESP8266

// Update these with values suitable for your network.

const char* ssid = "SSID";                                              // wifi name
const char* password = "PASSWORD";                                     // wifi password
const char* mqtt_server = "192.168.1.8";                              // mqtt broker IP
String device[DEVICELENGTH] = {"esp01","esp02","esp03","esp04",};    // change the device name to your liking but device id must be unique !!
int relays[DEVICELENGTH] = { 3, 9, 13, 12};                         // GPIO pins to be connected in the relay  
int switches[DEVICELENGTH] = { 10, 5, 14, 4};                      // GPIO pins to be connected in the switch  

WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
char msg[50];
int value = 0;
int i;
int tempState[DEVICELENGTH];
int previousState[DEVICELENGTH];

//Connecting to WiFi
void setup_wifi() 
{
  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    Serial.print(".");
    //Operate relay using physical switch while not connected to WiFi
    for(i = 0; i < DEVICELENGTH; i++)
       tempState[i] = switchStates(switches[i],relays[i],i,tempState[i]);
    
  }
  //  randomSeed(micros());/
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
// Loop until we're reconnected to MQTT broker
void reconnect() 
{
  while (!client.connected())
  {
    //Operate relay using physical switch while not connected to MQTT 
    for(i = 0; i < DEVICELENGTH; i++)
       tempState[i] = switchStates(switches[i],relays[i],i,tempState[i]);

    client.setSocketTimeout(2000);
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "test-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    //if you MQTT broker has clientID,username and password
    //please change following line to    if (client.connect(clientId,userName,passWord))
    if (client.connect(clientId.c_str()))
      {
        Serial.println(" connected");
        //once connected to MQTT broker, subscribe to topics and publish currentState
        for(i = 0; i < DEVICELENGTH; i++)
            subscription(device[i],relays[i]);
      } 
    else 
      {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
      }
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

// The callback for when a PUBLISH message is received from the server.
void callback(char* topic, byte* payload, unsigned int length)
{
  char message = payload[0];                        //convert byte to char
  Serial.print("Command from MQTT broker is : ");
  Serial.print(topic);
  Serial.print(" ");
  Serial.println(message);
  for(i = 0; i < DEVICELENGTH; i++)
   //if(strstr(topic,(char*) device[i].c_str()) != NULL)
     tempState[i] = compareTopic(topic,message,device[i],relays[i],i);
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
          return wifiLogic(true,relay,addr);  //Returns tempstate
      if(message == '0')
          return wifiLogic(false,relay,addr); //Returns tempstate
    }
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
int publishRelayChange(String deviceId,int relay,int previousState)
{
  int relayState = digitalRead(relay); 
  if(relayState != previousState)
  {
    String currentState = digitalRead(relay) == 0 ? "1" : "0";
    String topic = "device/" + deviceId + "/state";
    client.publish((char*) topic.c_str(), (char*) currentState.c_str());
    Serial.println(currentState + " published, deviceId: " + deviceId);
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
       EEPROM.write(addr, HIGH);
       EEPROM.commit();
      }
       return LOW;
    }
    if(digitalRead(button) == HIGH )
    {
      if(tempState == LOW)
      {
        digitalWrite(relay, 1);         //Relay off
        EEPROM.write(addr, LOW);
        EEPROM.commit();
      }
        return HIGH;
    }
  delay(100);
}
//To operate relay using WiFi
int wifiLogic(bool wifiState,int relay,int addr)
{
  if(wifiState == true)
    {
      digitalWrite(relay, 0);  //Relay on
      EEPROM.write(addr, HIGH);
      EEPROM.commit();
      return HIGH;
    }
  else
    {
      digitalWrite(relay, 1);  //Relay off
      EEPROM.write(addr, LOW);
      EEPROM.commit();
      return LOW;
    }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  EEPROM.begin(4);
  for(i = 0; i < DEVICELENGTH; i++)
  {
    pinMode(relays[i], OUTPUT);          // Declaring relay pins as output
    pinMode(switches[i], INPUT_PULLUP);  //Declaring switch pins as input (INPUT_PULLUP: For eliminating AC interference)
    digitalWrite(relays[i], 1);          //Set relay OFF initially (0=ON, 1=OFF)
  }

//Relay continue in previous state in case of power failure
for(i = 0; i < DEVICELENGTH; i++)
  {
    tempState[i] = EEPROM.read(i);
    tempState[i] = wifiLogic(tempState[i],relays[i],i); 
  }

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

 /////////////////////////////////OTA Update//////////////////////////////////

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
  
 /////////////////////////////////OTA Update//////////////////////////////////
}

void loop() {
  if (!client.connected())
    reconnect();
//To operate relay using physical switch
  for(i = 0; i < DEVICELENGTH; i++)
       tempState[i] = switchStates(switches[i],relays[i],i,tempState[i]);
       
//For publishing the state of relay using MQTT 
  for(i = 0; i < DEVICELENGTH; i++)
  previousState[i] = publishRelayChange(device[i],relays[i],previousState[i]);
  
  client.loop();
  ArduinoOTA.handle();
}
