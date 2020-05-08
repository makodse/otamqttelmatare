#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <PubSubClient.h>
#include <driver/adc.h>
#include "esp_adc_cal.h"
//otamq_elmatare - measure and report current to mqtt. based on other stuff, mostly mqtt client example and ota webserver.
#define ANTAL 10000
#define V_REF 1100


//sudo apt-install python-serial
//esp32 + libraries with the includes above...
//otamq_elmatare - monitors currenttransformers and posts to mqtt server.
int l1sensor=36;  //vp ch0
int l2sensor=39;  //un ch 3
int l3sensor=34;  //d34 ch6
int pvalue[ANTAL];

const char* host = "esp32";
const char* ssid = "wifi";
const char* password = "wifi-pass";
//mqtt server:
const char* mqtt_server = "192.168.3.220";

WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50]; 
int value = 0;

int period = 5000;
unsigned long time_now=0;
/*
 * Login page
 */

const char* loginIndex = 
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ESP32 Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='admin' && form.pwd.value=='admin')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";
 
/*
 * Server Index Page
 */
 
const char* serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";

/*
 * setup function
 */
void setup(void) {

  Serial.begin(115200);

  // Connect to WiFi network
  WiFi.begin(ssid, password);
  Serial.println("");

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

  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();
  client.setServer(mqtt_server, 1883);
  //client.setCallback(callback);
}
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("esp32/output");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
float getvalues(int fas)
{
  float amp =0;
  //hämta värden från angiven fas, returnera ampere.
  int count=0;
  uint32_t max=0, min=5000;

  adc1_config_width(ADC_WIDTH_12Bit);
  if (fas == 1){
      adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_11db);

  }else if (fas == 2) {
      adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_11db);
  
  }else{
      adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_11db);  
  }
  
  
  /*int l1sensor=36;  //vp ch0
  int l2sensor=39;  //un ch 3
  int l3sensor=34;  //d34 ch6*/

  // Calculate ADC characteristics i.e. gain and offset factors see elmatare_adc_test for help.
  esp_adc_cal_characteristics_t characteristics;
  esp_adc_cal_get_characteristics(V_REF, ADC_ATTEN_11db, ADC_WIDTH_12Bit, &characteristics);
  // Read ADC and obtain result in mV
  //uint32_t voltage = adc1_to_voltage(ADC1_CHANNEL_0, &characteristics);
  uint32_t voltage[ANTAL];
  unsigned long StartTime = millis();
  StartTime=StartTime +50;
  while((millis() <= StartTime) && count < ANTAL)
  {
     if (fas == 1){
      pvalue[count] = adc1_to_voltage(ADC1_CHANNEL_0, &characteristics);
  }else if (fas == 2) {
     pvalue[count] = adc1_to_voltage(ADC1_CHANNEL_3, &characteristics);
  
  }else{
     pvalue[count] = adc1_to_voltage(ADC1_CHANNEL_6, &characteristics);
  }
    
    count++;
  }
  int sma=0;
      for(int i=0; i<(count-1); i++){
        sma+=pvalue[i];
        
        if(i >= 10){
          sma-=pvalue[i-10];  
          if(min > (sma/10)){
            min=sma/10;
          }
          if(max < (sma/10)){
            max=sma/10;
          }
          
        }
      }
      amp=((((max - min)/2000.0)/1.41)/82.0)*2000;  
  return amp;  
}
void loop(void) {
  server.handleClient();
  delay(1);
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  if(millis() > time_now + period) {
  time_now=millis();
  
  //getvalues(l1sensor);
  //client.publish("el/L1", String(getvalues(l1sensor)));
  //client.publish("el/L2", String(getvalues(l2sensor)));
  //client.publish("el/L3", String(getvalues(l3sensor)));
  char res[10];
  float l1=0;
  float l2=0;
  float l3=0;
  float sum=0;
  l1=getvalues(1);
  dtostrf(l1, 0, 2, res);
  client.publish("el/L1", res);
  l2=getvalues(2);
  dtostrf(l2, 0, 2, res);
  client.publish("el/L2", res);
  l3=getvalues(3);
  dtostrf(l3, 0, 2, res);
  client.publish("el/L3", res);
  sum=(l1+l2+l3)*230;
  dtostrf(sum, 6, 2, res);
  client.publish("el/sum", res);
  
 //dtostrf(getvalues(l2sensor), 6, 2, res);
  //client.publish("el/L2", res);
  }
}
