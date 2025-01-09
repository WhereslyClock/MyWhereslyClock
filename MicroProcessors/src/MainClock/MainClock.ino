#include <FS.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager  2.0.3-alpha
#include <PubSubClient.h>  //2.8.0
#include <ArduinoJson.h> //5.13.5
#include <Wire.h>

#define TRIGGER_PIN 0

const int PIN_SCL = D1;
const int PIN_SDA = D2;
const int numPlaces = 8; //MinaLima Design, Film with 13 places is possible but overlaps in the segments too much, and mapping 8 
                        //1-6 is custom,  7 = In transit, 8 = Lost
const int stepsPerFace = 512; //Its not a face its the gap between digits on a clock, but this isnt a clock but if I say locations will that mix it with the gps and confuse, I dunno
const int stepsPerFaceAvail = 460; //90% of that  its like 5% padding either side
const int perPersonStep = 115; // Allows for a person offset so not all at exact position of location
const int stepFiller = 26; //Another filler, buggered if I can remember what for, see how handy comments are, you can have fun figuring it out  

WiFiManager wm; // global wm instance


char mqtt_port[4];// = "1883";
char mqtt_user[50];// = "";
char mqtt_password[50];// = "";
bool mqtt_secure;

char mqtt_id[20];
const char* mqtt_topic = "owntracks/OwnTracks/#";
const char* mqtt_topic_alive = "Wheresly";

//This is a pre-determined SSL hash to check against a dynamic server, cant go live with this.
//const char fingerprint[] PROGMEM = "";

char mqtt_server[70];

Client* client; // Pointer to base class

PubSubClient mqttclient;//(*client);

unsigned long lastAliveMessage;
const unsigned long aliveInterval = 10000; 
const unsigned long messageExpireInterval = 5000; // 1 seconds
unsigned long currentMillis; 
unsigned long lastClearMessage = 0;
bool aliveMessagePublished = false;

bool shouldSaveConfig = false;

//Mounst ESP File system and Load the Config File.
//Store Mqtt settings in mem.
void ensureFsMounted() {
    if (SPIFFS.begin()) {
        Serial.println("mounted file system");
        if (SPIFFS.exists("/config.json")) {
            //file exists, reading and loading
            Serial.println("reading config file");
            File configFile = SPIFFS.open("/config.json", "r");
            if (configFile) {
                Serial.println("opened config file");
                size_t size = configFile.size();
                // Allocate a buffer to store contents of the file.
                std::unique_ptr < char[] > buf(new char[size]);

                configFile.readBytes(buf.get(), size);
                DynamicJsonBuffer jsonBuffer;
                JsonObject & json = jsonBuffer.parseObject(buf.get());
                json.printTo(Serial);
                if (json.success()) {
                    Serial.println("\nparsed json");

                    strcpy(mqtt_server, json["mqtt_server"]);
                    strcpy(mqtt_port, json["mqtt_port"]);
                    strcpy(mqtt_user, json["mqtt_user"]);
                    strcpy(mqtt_password, json["mqtt_password"]);
                    mqtt_secure = atoi(json["mqtt_password"]);


                } else {
                    Serial.println("failed to load json config");
                }
                configFile.close();
            }
        }
    } else {
        Serial.println("failed to mount FS");
    }
}

//Get Mqtt Payload as String as I cant remember how to do basic C++ pointers to string stuff
String getPayload(byte* payload, int length) {

  String messageTemp;
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    messageTemp += (char)payload[i];
  }
  Serial.println("");
  return messageTemp;
}

//Mqtt Message Received
void callback(char* topic, byte* payload, unsigned int length) {
  String payString;
  int stp;
  int whichHand=0;
 
  bool eventHappened=false;

  DynamicJsonBuffer doc;
   
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  //Owntracks events,  the number represents a person, you need to set the number in the Owntracks settings,
  //This could have been aliased to Peoples Names but thats more settings in the Wifi so lets just hard code it now.
  if (strcmp(topic, "owntracks/OwnTracks/1/event")==0 || strcmp(topic, "owntracks/OwnTracks/1")==0 ){
    whichHand=1;eventHappened=true;
  }
  if (strcmp(topic, "owntracks/OwnTracks/2/event")==0 || strcmp(topic, "owntracks/OwnTracks/2")==0){
    whichHand=2;eventHappened=true;
      }
  if (strcmp(topic, "owntracks/OwnTracks/3/event")==0 || strcmp(topic, "owntracks/OwnTracks/3")==0){
    whichHand=3;eventHappened=true;    
  }
  if (strcmp(topic, "owntracks/OwnTracks/4/event")==0 || strcmp(topic, "owntracks/OwnTracks/4")==0){
    whichHand=4;eventHappened=true;    
  }

  if (eventHappened){
    
    JsonObject & json = doc.parseObject(payload);

    Serial.println("Event Happened - Here we Go");
    json.printTo(Serial);
    
    //transition is a type of event where the person enters or leaves a region, so you can scan enter/leave and 
    //the desc field contains the name of the region.   Those need to be called 1-6,  Home=1 etc,
    //Again I could have this as config, but thats a PITA, so hardcode for now.
    if(json["_type"]=="transition"){
      String stringDesc=json["desc"];
      int region=stringDesc.toInt();
      if (region>6) region=8; //lost, only allow 6 locajson.printTo(Serial);tions, 2 reserved. Cannot enter travelling thats a bodge.
      //The desc containes the name of the OwnTracks waypoint/region, these need to be named 1-N (where n is numPlaces-2) 7-8 reserved.
      if(json["event"] == "enter"){
        if(region==0) region = 8; //lost

        //512 steps per slot, but 4 people, so in 1st pos, range from 0-512 (if you set 512 youve already past it) so position 1 = 0 + personOffset 
        //but again 4th person is out of the box so range of hand pos needs to be 90% of that plus the starting point offset
        sendMoveToMotors(whichHand, ((region*stepsPerFace)-stepsPerFace) + ( whichHand * perPersonStep) + stepFiller);

        Serial.print("WhichHand-");
        Serial.println(whichHand);
        Serial.print("WhichPoint-");
        Serial.println(region);
        Serial.print("Positon to send");
        Serial.println(((region*stepsPerFace)-stepsPerFace) + ( whichHand * perPersonStep) + stepFiller);
        //TODO: StoreWHichHand location so not to bombard motors.
      }
      if(json["event"] == "leave"){
        //travelling
        region=7;//travelling
        
        Serial.print("WhichHand-");
        Serial.println(whichHand);
                Serial.print("WhichPoint-");
        Serial.println(region);
        Serial.print("Positon to send");
        Serial.println(((region*stepsPerFace)-stepsPerFace) + ( whichHand * perPersonStep) + stepFiller);
        
        sendMoveToMotors(whichHand, ((region*stepsPerFace)-stepsPerFace) + ( whichHand * perPersonStep) + stepFiller);     
      }
    }

    if(json["_type"]=="location"){
      String stringRegion=json["inregions"];   //No idea about this lib,  will it crash if this key doesnt exist.
      int region=stringRegion.toInt();
      if (region>6) region=8; //lost, only allow 6 locajson.printTo(Serial);tions, 2 reserved. Cannot enter travelling thats a bodge.
      if(region==0) region = 8; //lost
      sendMoveToMotors(whichHand, ((region*stepsPerFace)-stepsPerFace) + ( whichHand * perPersonStep) + stepFiller);

      Serial.print("WhichHand-");
      Serial.println(whichHand);
      Serial.print("WhichPoint-");
      Serial.println(region);
      Serial.print("Positon to send");
      Serial.println(((region*stepsPerFace)-stepsPerFace) + ( whichHand * perPersonStep) + stepFiller);
    }

  }
}

void sendMoveToMotors(int whichHand, int position){

  //This is a weird thing, it means you can stuff an interger into intNumber and it 
  //allows you to send the intBytes , so auto translating int to byte array. Handy.
  union Buffer
  {
    int intNumber;
    byte intBytes[2];
  };
  Buffer sndBuffer;

  byte numMotor;
  int device;

  //Reverse position as the motors are inverted
  //And multiple by two, because the steppers gear is half the size of the hand gears.
  //4096-0=4096
  //4096-4096=0;
  //4096-2048=2048
  position=((int)4096-position)*2;

  //Duh! gear Multiplier!!!
      
  sndBuffer.intNumber = position;

  //There are two Stepper Controllers, so talk to controller 1 or 2 and each controller has 1 or 2 motors
  if(whichHand==1){
    numMotor =1; 
    Wire.beginTransmission(1); // transmit to device #1
  } else if(whichHand == 2){
    numMotor=2;
    Wire.beginTransmission(1); // transmit to device #1  //Controller 1 listens to this
  } else if(whichHand== 3){
    numMotor=1;
    Wire.beginTransmission(2); // transmit to device #2  //Controller 2 listens to this
  } else if(whichHand == 4){
    numMotor=2;
    Wire.beginTransmission(2); // transmit to device #2
  }

  Wire.write(numMotor);
  Wire.write('T');                                //Tell the Controller to move a Hand T= travel, i dunno I just randomly picked a char
  Wire.write(sndBuffer.intBytes,2);               // send the int 2 bytes
  Wire.endTransmission();    // stop transmitting
 
  Serial.println(""); 
}


// Loop until we're reconnected to the MQTT server 
void reconnect() {
 while (!mqttclient.connected()) {
  Serial.print("Attempting MQTT connection...");
    checkButton();
  //Attempt to connect
  if (mqttclient.connect(mqtt_id, mqtt_user, mqtt_password)) {
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
 


void setup() {
  // Initialize random seed
  randomSeed(analogRead(0));

  // Generate a random number and append it to the mqtt_id
  snprintf(mqtt_id, sizeof(mqtt_id), "Clock-%04d", random(10000));


  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP  
  Serial.begin(9600);

  Serial.setDebugOutput(true);  
  delay(3000);

  //TODO: put some err handling here, what if this fails>
  ensureFsMounted();

  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 70);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 4);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 50);
  WiFiManagerParameter custom_mqtt_password("password", "mqtt password", mqtt_password, 50);
  WiFiManagerParameter custom_mqtt_secure("secure", "MQTT Secure", "0", 2); 

  Serial.println("\n Starting");

  
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  
  
  // test custom html(radio)
  wm.addParameter(&custom_mqtt_server);
  wm.addParameter(&custom_mqtt_port);
  wm.addParameter(&custom_mqtt_user);
  wm.addParameter(&custom_mqtt_password);
  wm.addParameter(&custom_mqtt_secure); 

  wm.setSaveConfigCallback(saveConfigCallback);

  bool res;
  //Default WiFi for clock config. 
  res = wm.autoConnect("Wheresley Clock","RubberDuck"); 

  if(!res) {
    Serial.println("Failed to connect or hit timeout");
    //TODO: We need some LEDS flashing colours and stuff, then restart on a button.
    // ESP.restart();
  } 
  else {
    //if you get here you have connected to the WiFi    
    Serial.println("connected...yeey :)");
    //TODO: Green LED on 
  
    //save the custom parameters to FS
    if (shouldSaveConfig) {
      Serial.println("saving config");

      strcpy(mqtt_server, custom_mqtt_server.getValue());
      strcpy(mqtt_port, custom_mqtt_port.getValue());
      strcpy(mqtt_user, custom_mqtt_user.getValue());
      strcpy(mqtt_password, custom_mqtt_password.getValue());
      mqtt_secure = atoi(custom_mqtt_secure.getValue());

      DynamicJsonBuffer jsonBuffer;

      JsonObject& json = jsonBuffer.createObject();
      json["mqtt_port"] = mqtt_port;
      json["mqtt_server"] = custom_mqtt_server.getValue();
      json["mqtt_user"] = mqtt_user;
      json["mqtt_password"] = mqtt_password;
      json["mqtt_secure"] = mqtt_secure;
  
      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        Serial.println("failed to open config file for writing");
      }
  
      json.printTo(Serial);
      json.printTo(configFile);
      configFile.close();
      //end save
    }

      
    //Connect to your MQTT Server and set callback
    
    //Bad Donkey!!  Change this to read any SSL by including certs.h and NTP etc  BearSSL
    //TODO:This is a huge job, and is there much point, it only allows for a MITM attack which is 
    //as rare as unicorn poop,  maybe one day.
    if (mqtt_secure) {
      Serial.println("Secure mqtt client");
      client = new WiFiClientSecure();
      //wifiClient.setInsecure();
      //wifiClient.setFingerprint(fingerprint);
    } else {
      Serial.println("Insecure mqtt client");
      client = new WiFiClient();
    }


    mqttclient.setClient(*client);
    mqttclient.setServer(mqtt_server, atoi(mqtt_port));
    mqttclient.setCallback(callback);

    
    Serial.println("Starting Wire");
    Wire.begin(PIN_SDA, PIN_SCL);
  }

  
  currentMillis = millis();
  lastAliveMessage = currentMillis;


}


void checkButton(){
  // check for button press
  if ( digitalRead(TRIGGER_PIN) == LOW ) {
    // poor mans debounce/press-hold, code not ideal for production
    delay(50);
    if( digitalRead(TRIGGER_PIN) == LOW ){
      Serial.println("Button Pressed");
      // still holding button for 3000 ms, reset settings, code not ideaa for production
      delay(3000); // reset delay hold
      if( digitalRead(TRIGGER_PIN) == LOW ){
        Serial.println("Button Held");
        Serial.println("Erasing Config, restarting");
        wm.resetSettings();
        WiFi.disconnect();
        wm.erase();
        SPIFFS.remove("/config.json");
        delay(500);
  
        ESP.restart();
      }     
    }
  }
}


void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


void loop() {

  currentMillis = millis();

  checkButton();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected!");
    WiFi.reconnect();
  }

  // put your main code here, to run repeatedly:
  if (!mqttclient.connected()) {
    Serial.println("reconnecting mqtt");
    reconnect();
  }
  else{

    // Publish "alive" message at the specified interval
    if ((currentMillis - lastAliveMessage) >= aliveInterval) {
      lastAliveMessage = currentMillis;
      mqttclient.publish(mqtt_topic_alive, "alive", true); // Publish with retain flag
      aliveMessagePublished = true; // Set the flag
      lastClearMessage = currentMillis; // Reset the clear message timer
    }

    // Clear the retained message after the specified interval if the "alive" message was published
    if (aliveMessagePublished && (currentMillis - lastClearMessage) >= messageExpireInterval) {
      mqttclient.publish(mqtt_topic_alive, "", true); // Publish empty message to clear retained message
      aliveMessagePublished = false; // Reset the flag
    }
  }
 
  mqttclient.loop();
  
}
