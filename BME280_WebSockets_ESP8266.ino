#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BMP280 bmp;

/* Wifi Credentials */

const char* ssid = "Jio4g";
const char* password = "Live.com";

unsigned int starttime = millis();
unsigned int endtime = starttime;

float temp;
float pres;
float alti;
float prev_pres;
float sea_pres;
String msg;
String msgproc;
uint8_t modep;
String c_arr[4];

static const String WeatherDef[26] = {"Settled Fine","Fine Weather","Becoming Fine","Fine Becoming Less Settled","Fine, Possibly showers","Fairly Fine, Improving","Fairly Fine, Possibly showers, early","Fairly Fine Showery Later","Showery Early, Improving","Changeable Mending","Fairly Fine , Showers likely","Rather Unsettled Clearing Later","Unsettled, Probably Improving","Showery Bright Intervals","Showery Becoming more unsettled","Changeable some rain","Unsettled, short fine Intervals","Unsettled, Rain later","Unsettled, rain at times","Very Unsettled, Finer at times","Rain at times, worse later","Rain at times, becoming very unsettled","Rain at Frequent Intervals","Very Unsettled, Rain","Stormy, possibly improving","Stormy, much rain"};

String processor(const String& var){
  if(var == "STATUS"){
    return String(msgproc);
  }
  else if(var == "TEMP"){
    return String(temp);
  }
  else if(var == "PRES"){
    return String(pres);
  }
  else if(var == "ALTI"){
    return String(alti);
  }
}

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

void notify() {
  ws.textAll(String(msg));
}

void receive_state(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (strcmp((char*)data, "Update") == 0) {
      prog();
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      break;
    case WS_EVT_DISCONNECT:
      break;
    case WS_EVT_DATA:
      receive_state(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }     
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void schdl_cleannupd(){
  endtime = millis();
  if((endtime - starttime) >= 5000) {
    starttime = millis();
    endtime = starttime;
    ws.cleanupClients();
  }
}

String msgprovider(float p, uint8_t x){
  const uint8_t WeatherType[32] = {1,2,4,8,15,18,21,22,24,1,2,5,11,14,16,19,23,25,26,1,2,3,6,7,9,10,12,13,17,20,25,26};
  String msg;
  float z=0;
  if(x==2){
    z=130-p/8.1;
  }
  else if(x==1){
    z=147-5*p/37.6;
  }
  else if(x==0){
    z=179-2*p/12.9;
  }
  int i=int(round(z)-1);
  if(i>31){
    msg = "Exception occured i: " + String(i);
  }
  else
    msg = WeatherDef[WeatherType[i]];
  return msg;
}

void prog(void){
  temp = bmp.readTemperature();
  pres = bmp.readPressure()/100;
  alti =bmp.readAltitude();
  sea_pres = (1-0.0065*alti/((temp-1.2)+0.0065*alti+273.15));
  sea_pres = pres*pow(sea_pres,-5.257);
  if(sea_pres>prev_pres && sea_pres-prev_pres>3){
    prev_pres=sea_pres;
    modep = 0;
  }
  else if(sea_pres==prev_pres or sea_pres-prev_pres<3){
    modep = 1;
  }
  else if(sea_pres<prev_pres && prev_pres-sea_pres>3){
    prev_pres=sea_pres;
    modep = 2;
  }
  msgproc=msgprovider(sea_pres,modep);
  msg=msgproc;
  if(c_arr[0]!=String(temp)){
    msg = "t?" + String(temp);
    notify();
    c_arr[0]=String(temp);
  }
  if(c_arr[1]!=String(pres)){
    msg = "p?" + String(pres);
    notify();
    c_arr[1]=String(pres);
  }
  if(c_arr[2]!=String(alti)){
    msg = "a?" + String(alti);
    notify();
    c_arr[2]=String(alti);
  }
  if(!strcmp(c_arr[3].c_str(),msg.c_str())){
    c_arr[3]=msg;
    msg = "m?" + msg;
    notify();
  }
}

void setup(void) {
  if(!SPIFFS.begin()){
     Serial.println("An Error has occurred while mounting SPIFFS");
     return;
  }
  WiFi.begin(ssid, password);
  bmp.begin(0x76);
  Serial.begin(57600);
  Serial.println(" ");
  while(WiFi.status() != WL_CONNECTED) {
    Serial.print("o");
    delay(500);
  }
  Serial.println(" ");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  initWebSocket();
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/webpage.html", String(), false, processor);
  });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/favicon.ico", "image/ico");
  });
  server.begin();
  prev_pres=bmp.readPressure()/100;
  float val=(1-0.0065*bmp.readAltitude()/((bmp.readTemperature()-1.2)+0.0065*bmp.readAltitude()+273.15));
  prev_pres =prev_pres*pow(val, -5.257);
}

void loop(void) {
    prog();
    delay(10000);
    schdl_cleannupd();
}
