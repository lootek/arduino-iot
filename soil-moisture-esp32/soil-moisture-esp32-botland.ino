//#include <SPI.h>
//#include <Wire.h>
//#include "WiFi.h"
//#include "ESPAsyncWebServer.h"
//#include <Adafruit_Sensor.h>
//#include <DHT.h>
//
//const char* ssid = "";
//const char* password = "";
//
//#define DHTPIN 22
//#define DHTTYPE    DHT11
//DHT dht(DHTPIN, DHTTYPE);
//
//AsyncWebServer server(80);
//
//String readDHTTemperature() {
//  float t = dht.readTemperature();
//  if (isnan(t)) {
//    Serial.println("Temp measurement error");
//    return "--";
//  }
//  else {
//    Serial.println(t);
//    return String(t);
//  }
//}
//
//String readDHTHumidity() {
//  float h = dht.readHumidity();
//  if (isnan(h)) {
//    Serial.println("Humidity measurement error");
//    return "--";
//  }
//  else {
//    Serial.println(h);
//    return String(h);
//  }
//}
//
//const char index_html[] PROGMEM = R"rawliteral(
//<!DOCTYPE HTML><html>
//<head>
//  <meta name="vewport" content="width=device-width, initial-scale=1">
//  <link data-minify="1" rel="stylesheet" href="https://botland.com.pl/blog/wp-content/cache/min/1/releases/v5.7.2/css/all.css?ver=1698648818" crossorigin="anonymous">
//  <style>
//    html {
//     font-family: Arial;
//     display: inline-block;
//     margin: 0px auto;
//     text-align: center;
//    }
//    h2 {font-size: 3.0rem;}
//    p {font-size: 3.0rem;}
//    .units {font-size: 1.2rem;}
//    .dht-labels{
//     font-size: 1.5rem;
//     vertical-align: middle;
//     padding-bottom: 15px;
//    }
//    </style>
//</head>
//<body>
//  <h2><span id="Serwer-ESP32">Serwer ESP32</span></h2>
//  <p>
//    <i class="fas fa-thermometer-three-quarters" style="color:#666;"></i>
//    <span class="dht-labels">Temperatura</span>
//    <span id="temperature">%TEMPERATURE%</span>
//    <sup class="units">&deg;C</sup>
//  </p>
//  <p>
//    <i class="fas fa-tint" style="color:#666;"></i>
//    <span class="dht-labels">Wilgotnosc</span>
//    <span id="humidity"%HUMIDITY%></span>
//    <sup class="units">&percnt;</sup>
//  </p>
//<script>class RocketElementorAnimation{constructor(){this.deviceMode=document.createElement("span"),this.deviceMode.id="elementor-device-mode-wpr",this.deviceMode.setAttribute("class","elementor-screen-only"),document.body.appendChild(this.deviceMode)}_detectAnimations(){let t=getComputedStyle(this.deviceMode,":after").content.replace(/"/g,"");this.animationSettingKeys=this._listAnimationSettingsKeys(t),document.querySelectorAll(".elementor-invisible[data-settings]").forEach(t=>{const e=t.getBoundingClientRect();if(e.bottom>=0&&e.top<=window.innerHeight)try{this._animateElement(t)}catch(t){}})}_animateElement(t){const e=JSON.parse(t.dataset.settings),i=e._animation_delay||e.animation_delay||0,n=e[this.animationSettingKeys.find(t=>e[t])];if("none"===n)return void t.classList.remove("elementor-invisible");t.classList.remove(n),this.currentAnimation&&t.classList.remove(this.currentAnimation),this.currentAnimation=n;let s=setTimeout(()=>{t.classList.remove("elementor-invisible"),t.classList.add("animated",n),this._removeAnimationSettings(t,e)},i);window.addEventListener("rocket-startLoading",function(){clearTimeout(s)})}_listAnimationSettingsKeys(t="mobile"){const e=[""];switch(t){case"mobile":e.unshift("_mobile");case"tablet":e.unshift("_tablet");case"desktop":e.unshift("_desktop")}const i=[];return["animation","_animation"].forEach(t=>{e.forEach(e=>{i.push(t+e)})}),i}_removeAnimationSettings(t,e){this._listAnimationSettingsKeys().forEach(t=>delete e[t]),t.dataset.settings=JSON.stringify(e)}static run(){const t=new RocketElementorAnimation;requestAnimationFrame(t._detectAnimations.bind(t))}}document.addEventListener("DOMContentLoaded",RocketElementorAnimation.run);</script></body>
//<script>
//setInterval(function ( ) {
//  var xhttp = new XMLHttpRequest();
//  xhttp.onreadystatechange = function() {
//    if (this.readyState == 4 && this.status == 200) {
//      document.getElementById("temperature").innerHTML = this.responseText;
//    }
//  };
//  xhttp.open("GET", "/temperature", true);
//  xhttp.send();
//}, 10000 ) ;
//
//setInterval(function ( ) {
//  var xhttp = new XMLHttpRequest();
//  xhttp.onreadystatechange = function() {
//    if (this.readyState == 4 && this.status == 200) {
//      document.getElementById("humidity").innerHTML = this.responseText;
//    }
//  };
//  xhttp.open("GET", "/humidity", true);
//  xhttp.send();
//}, 10000 ) ;
//</script>
//</html>)rawliteral";
//
//String processor(const String& var) {
//  if (var == "TEMPERATURE") {
//    return readDHTTemperature();
//  }
//  else if (var == "HUMIDITY") {
//    return readDHTHumidity();
//  }
//  return String();
//}
//
//void setup() {
//  Serial.begin(115200);
//
//  dht.begin();
//
//  WiFi.begin(ssid, password);
//  while (WiFi.status() != WL_CONNECTED) {
//    delay(1000);
//    Serial.println("Connecting to WiFi..." );
//  }
//
//  Serial.println(WiFi.localIP());
//
//  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
//    request->send_P(200, "text/html", index_html, processor);
//  });
//  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest * request) {
//    request->send_P(200, "text/plain", readDHTTemperature().c_str());
//  });
//  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest * request) {
//    request->send_P(200, "text/plain", readDHTHumidity().c_str());
//  });
//
//  server.begin();
//}
//void loop() {
//  float t = dht.readTemperature();
//  float h = dht.readHumidity();
//  delay(5000);
//}
