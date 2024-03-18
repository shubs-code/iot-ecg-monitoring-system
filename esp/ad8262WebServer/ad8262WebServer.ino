
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>


#define LED 13
const int led = 2;
const int buzzer = 23;

int IRAM_ATTR ECG_RAW[10][500];
int IRAM_ATTR buzzer_millis = 0;
// reading from heart sensor.
hw_timer_t *My_timer = NULL;
void IRAM_ATTR onTimer(){
  //Read value.
  int sec = millis() / 1000;
  int arrayInd = sec%10;
  int readingInd = (millis()%1000)/2;
  int raw_ecg_data = analogRead(34);
  ECG_RAW[arrayInd][readingInd] = raw_ecg_data; // ecg pin attached to pin 34.
  if(raw_ecg_data >=3500){
    digitalWrite(buzzer, 1);
    buzzer_millis = millis();
  }else if(millis() - buzzer_millis >=100){
    digitalWrite(buzzer, 0);
  }
}

// web server 
const char *ssid = "SS";
const char *password = "chromosome";

WebServer server(80);


void setup(void) {
  pinMode(led, OUTPUT);
  pinMode(buzzer, OUTPUT);
  digitalWrite(led, 0);
  digitalWrite(buzzer, 0);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  //set up interupts for sensor reading.
  pinMode(LED, OUTPUT);
  My_timer = timerBegin(0, 80, true);
  timerAttachInterrupt(My_timer, &onTimer, true);
  timerAlarmWrite(My_timer, 2000, true);
  timerAlarmEnable(My_timer); //Just Enable
  
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("ecg")) {
    Serial.println("MDNS responder started");
  }
  server.enableCORS(true);
  server.enableCrossOrigin(true);
  server.on("/", handleRoot);
  server.on("/raw", sendECG);
  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) { 
  server.handleClient();
  delay(2);//allow the cpu to switch to other tasks
}

void handleRoot() {
  digitalWrite(led, 1);
  char temp[400];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  snprintf(temp, 400,

           "<html>\
  <head>\
    <meta http-equiv='refresh' content='1'/>\
    <title>ECG SYSTEM</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>ECG SENSOR</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
  </body>\
</html>",

           hr, min % 60, sec % 60
          );
  server.send(200, "text/html", temp);
  digitalWrite(led, 0);
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}


void sendECG(){
  int sec = millis() / 1000;
  int arrayInd = sec%10;
  int newData = 1;
  if(server.args() > 0){
    int prevSec =  server.arg(0).toInt();
    Serial.println(String(prevSec)+" <-> "+String(arrayInd));
    if(prevSec == arrayInd){
      Serial.println("same data "+String(sec));
      newData = 0;
    }else{
      arrayInd = (prevSec+1)%10;
    }
  }
  else if(arrayInd>0){ arrayInd--;}
  else{arrayInd = 9;}
  String out = "{\"sec\":"+String(arrayInd)+",";
  out+="\"newData\":"+String(newData);
  if(newData==0){
    out+= "}";
  }else{
    out += ",\"data\":[";
    for(int i = 0 ; i < 499 ; i++){
      out += String(ECG_RAW[arrayInd][i])+",";
    }out += String(ECG_RAW[arrayInd][499])+"]}  "; 
  }
  
  server.send(200, "application/json", out);
}
