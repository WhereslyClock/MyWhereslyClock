// Example certificate thumbprint validation to create
// a secure connection to a MQTT broker!
//
// Mar 2019 by Jesse Bedard
// Released to the public domain

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Define Your Settings
const char* ssid = "MySSID";
const char* password = "password";
const char* mqtt_server = "my mqtt server";
const char* mqtt_username = "clock";
const char* mqtt_password = "password";
const char* mqtt_clientname = "clock";
const int mqtt_port = 8883;
char* mqtt_topic = "owntracks/OwnTracks/#";
const char fingerprint[] PROGMEM = "Get the SSL fingerprint here";

//Simple boolean to indicate first startup loop
bool startup = false;

//Define WifiClientSecure
WiFiClientSecure client; 
PubSubClient mqttclient(client);
 
 
void callback(char* topic, byte* payload, unsigned int length) {
 Serial.print("Message arrived [");
 Serial.print(topic);
 Serial.print("] ");
 for (int i=0;i<length;i++) {
    char receivedChar = (char)payload[i];
    Serial.print(receivedChar);
    //****
    //Do some action based on message received
    //***
 }
  Serial.println("");
}
 
 
void reconnect() {
 // Loop until we're reconnected to the MQTT server
 while (!mqttclient.connected()) {
  Serial.print("Attempting MQTT connection...");
  //Attempt to connect
  if (mqttclient.connect(mqtt_clientname, mqtt_username, mqtt_password)) {
    Serial.println("connected");
    //Subscribe to topic
    mqttclient.subscribe(mqtt_topic);
  } else {
    Serial.print("failed, rc=");
    Serial.print(mqttclient.state());
    Serial.println(" try again in 5 seconds");
    // Wait 5 seconds before retrying
    delay(5000);
  } //if
 }//while
}
 
void setup()
{
  Serial.begin(115200);  
  WiFi.begin(ssid, password);
  //****
  //Important to set fingerprint to verify server
  //setInsecure() will allow the ssl connection without verification
  //****
  //client.setInsecure(); //Do NOT verify server!!!
  client.setFingerprint(fingerprint);
  
  
  Serial.println("Connecting to WiFi.");
  int _try = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    _try++;  
  }
  Serial.println("Connected to the WiFi network");  

  //Connect to your MQTT Server and set callback
  mqttclient.setServer(mqtt_server, mqtt_port);
  mqttclient.setCallback(callback);

}
 
void loop()
{
  if (!mqttclient.connected()) {
    reconnect();
  }

  //Publish a startup message
  if (startup == false){
    Serial.print("Publish startup message 1");
    mqttclient.publish(mqtt_topic, "1",true);
    startup = true;
  };
 
  mqttclient.loop();
}
