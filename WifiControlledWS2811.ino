#include <WiFi.h>
#include <FastLED.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

#define LED_PIN     5
#define MAX_LEDS    300  // Maximum number of LEDs the strip can have
#define BRIGHTNESS  64
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
CRGB leds[MAX_LEDS];

const char* ssid = "Sognav";
const char* password = "2%Lemonade";
IPAddress staticIP(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0); 

AsyncWebServer server(80);

enum Mode { OFF, RAINBOW, COLOR_WIPE, THEATER_CHASE, SOLID_COLOR, KITS_LIGHT, PULSE, TWINKLE, FIRE };
Mode currentMode = OFF;

int brightness = 128;
int speed = 50;
int numLeds = 60;  // Default number of LEDs
CRGB solidColor = CRGB::White;
bool modeChanged = false;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 LED Control</title>
  <style>
    body { font-family: Arial; text-align: center; background-color: #202020; color: white; }
    .slider { -webkit-appearance: none; width: 80%; height: 15px; background: #d3d3d3; outline: none; opacity: 0.7; -webkit-transition: .2s; transition: opacity .2s; }
    .slider:hover { opacity: 1; }
    .slider::-webkit-slider-thumb { -webkit-appearance: none; appearance: none; width: 25px; height: 25px; background: #4CAF50; cursor: pointer; }
    .slider::-moz-range-thumb { width: 25px; height: 25px; background: #4CAF50; cursor: pointer; }
    .button { display: inline-block; padding: 10px 20px; font-size: 20px; cursor: pointer; text-align: center; outline: none; color: #fff; background-color: #4CAF50; border: none; border-radius: 15px; box-shadow: 0 9px #999; }
    .button:hover { background-color: #3e8e41 }
    .button:active { background-color: #3e8e41; box-shadow: 0 5px #666; transform: translateY(4px); }
    .color-button { display: inline-block; padding: 10px; margin: 5px; width: 40px; height: 40px; border-radius: 50%; }
  </style>
</head>
<body>
  <h2>ESP32 LED Strip Control</h2>
  <label for="brightness">Brightness:</label>
  <input type="range" id="brightness" name="brightness" min="0" max="255" class="slider" value="128">
  <br>
  <label for="speed">Speed:</label>
  <input type="range" id="speed" name="speed" min="1" max="100" class="slider" value="50">
  <br>
  <label for="numLeds">Number of LEDs:</label>
  <input type="number" id="numLeds" name="numLeds" min="1" max="300" value="60">
  <button class="button" onclick="setNumLeds()">Set</button>
  <br>
  <button class="button" onclick="setMode('rainbow')">Rainbow</button>
  <button class="button" onclick="setMode('colorWipe')">Color Wipe</button>
  <button class="button" onclick="setMode('theaterChase')">Theater Chase</button>
  <button class="button" onclick="setMode('kitsLight')">Kit's Light</button>
  <button class="button" onclick="setMode('pulse')">Pulse</button>
  <button class="button" onclick="setMode('twinkle')">Twinkle</button>
  <button class="button" onclick="setMode('fire')">Fire</button>
  <br>
  <div>
    <div class="color-button" style="background-color: #FF0000;" onclick="setColor('red')"></div>
    <div class="color-button" style="background-color: #00FF00;" onclick="setColor('green')"></div>
    <div class="color-button" style="background-color: #0000FF;" onclick="setColor('blue')"></div>
    <div class="color-button" style="background-color: #FFFF00;" onclick="setColor('yellow')"></div>
    <div class="color-button" style="background-color: #FFA500;" onclick="setColor('orange')"></div>
    <div class="color-button" style="background-color: #800080;" onclick="setColor('purple')"></div>
    <div class="color-button" style="background-color: #00FFFF;" onclick="setColor('cyan')"></div>
    <div class="color-button" style="background-color: #FFC0CB;" onclick="setColor('pink')"></div>
    <div class="color-button" style="background-color: #FFFFFF;" onclick="setColor('white')"></div>
    <div class="color-button" style="background-color: #000000;" onclick="setColor('black')"></div>
    <div class="color-button" style="background-color: #808080;" onclick="setColor('gray')"></div>
    <div class="color-button" style="background-color: #800000;" onclick="setColor('maroon')"></div>
    <div class="color-button" style="background-color: #808000;" onclick="setColor('olive')"></div>
    <div class="color-button" style="background-color: #008000;" onclick="setColor('darkgreen')"></div>
    <div class="color-button" style="background-color: #000080;" onclick="setColor('navy')"></div>
  </div>
  <br>
  <script>
    var brightnessSlider = document.getElementById("brightness");
    var speedSlider = document.getElementById("speed");
    var numLedsInput = document.getElementById("numLeds");

    brightnessSlider.oninput = function() {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/setBrightness?value=" + this.value, true);
      xhr.send();
    }

    speedSlider.oninput = function() {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/setSpeed?value=" + this.value, true);
      xhr.send();
    }

    function setMode(mode) {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/setMode?mode=" + mode, true);
      xhr.send();
    }

    function setColor(color) {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/setColor?color=" + color, true);
      xhr.send();
    }

    function setNumLeds() {
      var numLeds = numLedsInput.value;
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/setNumLeds?num=" + numLeds, true);
      xhr.send();
    }
  </script>
</body>
</html>)rawliteral";

void setup() {
  // Initialize serial and wait for port to open:
  Serial.begin(115200);
  
  // Connect to Wi-Fi
  WiFi.config(staticIP, gateway, subnet);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Initialize FastLED
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, MAX_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(brightness);

  // Route to load the web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // Route to set the brightness
  server.on("/setBrightness", HTTP_GET, [](AsyncWebServerRequest *request){
    String value = request->getParam("value")->value();
    brightness = value.toInt();
    FastLED.setBrightness(brightness);
    modeChanged = true;
    request->send(200, "text/plain", "OK");
  });

  // Route to set the speed
  server.on("/setSpeed", HTTP_GET, [](AsyncWebServerRequest *request){
    String value = request->getParam("value")->value();
    speed = value.toInt();
    modeChanged = true;
    request->send(200, "text/plain", "OK");
  });

  // Route to set the mode
  server.on("/setMode", HTTP_GET, [](AsyncWebServerRequest *request){
    String mode = request->getParam("mode")->value();
    if (mode == "rainbow") currentMode = RAINBOW;
    else if (mode == "colorWipe") currentMode = COLOR_WIPE;
    else if (mode == "theaterChase") currentMode = THEATER_CHASE;
    else if (mode == "kitsLight") currentMode = KITS_LIGHT;
    else if (mode == "pulse") currentMode = PULSE;
    else if (mode == "twinkle") currentMode = TWINKLE;
    else if (mode == "fire") currentMode = FIRE;
    else currentMode = OFF;
    modeChanged = true;
    request->send(200, "text/plain", "OK");
  });

  // Route to set the color
  server.on("/setColor", HTTP_GET, [](AsyncWebServerRequest *request){
    String color = request->getParam("color")->value();
    if (color == "red") solidColor = CRGB::Red;
    else if (color == "green") solidColor = CRGB::Green;
    else if (color == "blue") solidColor = CRGB::Blue;
    else if (color == "yellow") solidColor = CRGB::Yellow;
    else if (color == "orange") solidColor = CRGB::Orange;
    else if (color == "purple") solidColor = CRGB::Purple;
    else if (color == "cyan") solidColor = CRGB::Cyan;
    else if (color == "pink") solidColor = CRGB::DeepPink;
    else if (color == "white") solidColor = CRGB::White;
    else if (color == "black") solidColor = CRGB::Black;
    else if (color == "gray") solidColor = CRGB::Gray;
    else if (color == "maroon") solidColor = CRGB::Maroon;
    else if (color == "olive") solidColor = CRGB::Olive;
    else if (color == "darkgreen") solidColor = CRGB::DarkGreen;
    else if (color == "navy") solidColor = CRGB::Navy;
    currentMode = SOLID_COLOR;
    modeChanged = true;
    request->send(200, "text/plain", "OK");
  });

  // Route to set the number of LEDs
  server.on("/setNumLeds", HTTP_GET, [](AsyncWebServerRequest *request){
    String num = request->getParam("num")->value();
    numLeds = num.toInt();
    if (numLeds > MAX_LEDS) numLeds = MAX_LEDS;
    modeChanged = true;
    request->send(200, "text/plain", "OK");
  });

  server.begin();
}

void loop() {
  if (modeChanged) {
    FastLED.clear();
    modeChanged = false;
  }
  
  switch (currentMode) {
    case RAINBOW:
      rainbow();
      break;
    case COLOR_WIPE:
      colorWipe(solidColor);
      break;
    case THEATER_CHASE:
      theaterChase(solidColor);
      break;
    case KITS_LIGHT:
      kitsLight();
      break;
    case PULSE:
      pulse(solidColor);
      break;
    case TWINKLE:
      twinkle();
      break;
    case FIRE:
      fire();
      break;
    case SOLID_COLOR:
      fill_solid(leds, numLeds, solidColor);
      FastLED.show();
      break;
    case OFF:
    default:
      FastLED.clear();
      FastLED.show();
      break;
  }

  delay(1000 / speed);
}

// Animation functions

void rainbow() {
  static uint8_t hue = 0;
  fill_rainbow(leds, numLeds, hue, 7);
  FastLED.show();
  hue++;
}

void colorWipe(CRGB color) {
  static int ledIndex = 0;
  if (ledIndex < numLeds) {
    leds[ledIndex] = color;
    FastLED.show();
    ledIndex++;
  } else {
    ledIndex = 0;
  }
}

void theaterChase(CRGB color) {
  static int counter = 0;
  for (int i = 0; i < numLeds; i++) {
    if ((i + counter) % 3 == 0) {
      leds[i] = color;
    } else {
      leds[i] = CRGB::Black;
    }
  }
  FastLED.show();
  counter++;
  if (counter >= 3) counter = 0;
}

void kitsLight() {
  static int position = 0;
  static int direction = 1;
  FastLED.clear();
  leds[position] = solidColor;
  FastLED.show();
  position += direction;
  if (position == numLeds - 1 || position == 0) {
    direction *= -1;
  }
}

void pulse(CRGB color) {
  static uint8_t brightness = 0;
  static int8_t direction = 1;
  fill_solid(leds, numLeds, color.nscale8(brightness));
  FastLED.show();
  brightness += direction;
  if (brightness == 0 || brightness == 255) {
    direction *= -1;
  }
}

void twinkle() {
  FastLED.clear();
  for (int i = 0; i < numLeds / 10; i++) {
    leds[random(numLeds)] = CHSV(random(255), 255, 255);
  }
  FastLED.show();
  delay(50);
}

void fire() {
  /*
  static byte heat[MAX_LEDS];
  for (int i = 0; i < numLeds; i++) {
    heat[i] = max(0, heat[i] - random(0, ((55 * 10) / numLeds) + 2));
  }
  for (int i = numLeds - 1; i >= 2; i--) {
    heat[i] = (heat[i - 1] + heat[i - 2] + heat[i - 2]) / 3;
  }
  for (int i = 0; i < numLeds; i++) {
    CRGB color = HeatColor(heat[i]);
    int pixel = (numLeds - 1) - i;
    leds[pixel] = color;
  }
  FastLED.show();
  delay(1000 / speed);
  */
}