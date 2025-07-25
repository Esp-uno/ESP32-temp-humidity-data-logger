#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

// Libraries for the ESP32
#include <Arduino.h>
#include <WiFi.h>
#include <DHT.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <time.h>

//Network credentials
#define wifi_ssid ""
#define wifi_password ""

//firebase api key and URL
#define web_api_key ""
#define database_url ""

//auth email and password
#define user_email ""
#define user_password ""

#define DHT_PIN 18
#define DHT_TYPE DHT11

void processData(AsyncResult &aResult);
UserAuth user_auth(web_api_key, user_email, user_password);

//Firebase components
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;

//store timestamp of the last data transmission
unsigned long lastsendtime = 0;

//interval between data transmissions = 10 seconds
const unsigned long sendinterval = 10000;

//Save user UID
String uid;

//Database path
String database_path;

//Childnodes
String temperature_path = "/temperature";
String humidity_path = "/humidity";
String time_path = "/time";                                                                 

//parentnode
String parentpath;
int timestamp;
const char* ntpServer = "pool.ntp.org";

//Object containing pin and sensor type
DHT dht(DHT_PIN,DHT_TYPE);

//Holds the temp and humidiity data from the dht11 sensor
float temperature;
float humidity;

//JSON objects for storing data
object_t jsonData, obj1, obj2, obj3;
JsonWriter writer;

//initialise DHT
void Initialise_dht(){
    dht.begin();
    Serial.println("DHT11 sensor is initialised");
    delay(2000);
}

//Connect to network
void connect_to_wifi(){
    Serial.print("Connecting to wifi");

    WiFi.begin(wifi_ssid,wifi_password);

    while(WiFi.status()!= WL_CONNECTED)
    {
        delay(1000);
        Serial.print(".");
    }

    Serial.println();
    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());
}

//Unix time
unsigned long getTime(){
    time_t now;
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
        return(0);
    };
    time(&now);
    return now;
}


//initialise firebase
void initialise_firebase(){
    initializeApp(aClient, app, getAuth(user_auth), processData, "🔐 authTask");
    app.getApp<RealtimeDatabase>(Database);
    Database.url(database_url);
    
}

void processData(AsyncResult &aResult) {
    if (aResult.isError()) {
        Serial.print("Error: ");
        Serial.println(aResult.error().message());
    }
    if (aResult.available()) {
        Serial.println("Data sent successfully!");
    }
}

void setup(){
    Serial.begin(115200);

    Initialise_dht();
    connect_to_wifi();
    configTime(0,0,ntpServer);

    //configure ssl client
    ssl_client.setInsecure();
    ssl_client.setTimeout(1000);
    ssl_client.setHandshakeTimeout(5);
    
    initialise_firebase();
}

void loop() {
    //check authentication
    app.loop();
    if (app.ready()){
            //Send data every 10 seconds
            unsigned long currentTime = millis();
            if (currentTime - lastsendtime >= sendinterval){
                //update last send time
                lastsendtime = currentTime;

                uid = app.getUid().c_str();
                database_path.reserve(100);  // Pre-allocate memory
                parentpath.reserve(120);     

                //update database path
                database_path = "/UsersData/" +uid+"/readings";

                //get current timestamps
                timestamp = getTime();
                Serial.print("time: ");
                Serial.println(timestamp);

                parentpath= database_path + "/"+String(timestamp);

                //get sensor readings
                temperature = dht.readTemperature();
                humidity = dht.readHumidity();

                if (isnan(humidity) || isnan(temperature)){
                    Serial.println("Failed to read from sensor");
                    return;
                }
                
                Serial.print("Temperature: ");
                Serial.print(temperature);
                Serial.print("C, Humidity: ");
                Serial.print(humidity);
                Serial.println("%");
                
                //Create a JSON object with the data 
                writer.create(obj1, "/temperature", temperature);
                writer.create(obj2, "/humidity", humidity);
                writer.create(obj3, "/time", timestamp);
                writer.join(jsonData, 3, obj1, obj2, obj3);
                
                Database.set<object_t>(aClient, parentpath, jsonData, processData, "RTBD_SEND_DATA");
        }
    }
}