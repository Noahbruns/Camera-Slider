/* Create a WiFi access point and provide a web server on it. */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>        // Include the mDNS library
#include "html.h"

/* Slider */
#define EN_PIN    2  //  D4 
#define DIR_PIN   4  //  D2 
#define STEP_PIN  0  //  D3  
#define STOP_PIN  5  //  D1   
#define CS_PIN    15 // D8   
#define MOSI_PIN  13 //  D7
#define MISO_PIN  12 //   D6
#define SCK_PIN   14 //   D5 

constexpr uint32_t microseps    = 256; 
constexpr float steps_per_mm_float = 6.83;
constexpr float max_speed = 20; // mm/s
constexpr float acceleration = 10; // mm/s²
constexpr uint32_t steps_per_mm = steps_per_mm_float * microseps;

uint32_t pos_up = 1100;
uint32_t pos_down = 0;

bool stop = false;

#include <TMC2130Stepper.h>
#include <TMC2130Stepper_REGDEFS.h>
TMC2130Stepper driver = TMC2130Stepper(EN_PIN, DIR_PIN, STEP_PIN, CS_PIN, MOSI_PIN, MISO_PIN, SCK_PIN);

#include <AccelStepper.h>
AccelStepper stepper = AccelStepper(stepper.DRIVER, STEP_PIN, DIR_PIN);


/* WIFI */

#define APSSID "freizeitheim"
#define APPSK  "ade592bca2"

/* Set these to your desired credentials. */
const char *ssid = APSSID;
const char *password = APPSK;

ESP8266WebServer server(80);

bool bounce = false;

/* Just a little test message.  Go to http://192.168.4.1 in a web browser
   connected to this access point to see it.
*/
void handleRoot() {
  server.send(200, "text/html", html_index_html);
}

void handleVal() {
  //Serial.println(server.arg("q"));
  long val = stepper.currentPosition() / steps_per_mm;

  char str[40];
  sprintf(str, "%ld", val);
  Serial.println(str);

  server.send(200, "text/html", str);
}

void handlegoUp() {
  stepper.enableOutputs();
  stepper.moveTo(steps_per_mm * pos_up);

  server.send(200, "text/html", "OK");
}

void handlegoDown() {
  stepper.enableOutputs();
  stepper.moveTo(steps_per_mm * pos_down);

  server.send(200, "text/html", "OK");
}

void handleSetUp() {
  pos_up = server.arg("q").toInt();

  server.send(200, "text/html", "OK");
}

void handleSetDown() {
  pos_down = server.arg("q").toInt();

  server.send(200, "text/html", "OK");
}

void handleSetBounce() {
  bounce = true;

  server.send(200, "text/html", "OK");
} 

void handleSetNoBounce() {
  bounce = false;

  server.send(200, "text/html", "OK");
} 

void handleSpeed() {
  if (server.hasArg("q")) {
    long speed = server.arg("q").toInt();
    stepper.setMaxSpeed(speed * steps_per_mm / 10);
  }

  server.send(200, "text/html", String(stepper.maxSpeed() * 10 / steps_per_mm));
}

void handleStop() {
  bounce = false;
  stop = true;
  stepper.stop();

  server.send(200, "text/html", "OK");
} 


void setup() {
  delay(1000);
  SPI.begin();
  Serial.begin(115200);

  Serial.println();
  Serial.print("Connecting:");
  Serial.print(ssid);

  // Connect to WiFi
  WiFi.begin(ssid, password);

  // while wifi not connected yet, print '.'
  // then after it connected, get out of the loop
  while (WiFi.status() != WL_CONNECTED) {
     delay(500);
     Serial.print(".");
  }

  if (!MDNS.begin("slider")) {             // Start the mDNS responder for esp8266.local
    Serial.println("Error setting up MDNS responder!");
  }
  Serial.println("mDNS responder started");

  IPAddress myIP = WiFi.localIP();
  Serial.print("IP address: ");
  Serial.println(myIP);

  server.on("/", handleRoot);
  server.on("/val", handleVal);
  server.on("/goUp", handlegoUp);
  server.on("/goDown", handlegoDown);
  server.on("/SetUp", handleSetUp);
  server.on("/SetDown", handleSetDown);
  server.on("/Bounce", handleSetBounce);
  server.on("/noBounce", handleSetNoBounce);
  server.on("/Speed", handleSpeed);
  server.on("/Stop", handleStop);

  server.begin();
  Serial.println("HTTP server started");

  Serial.println("Start...");
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  driver.begin();             // Initiate pins and registeries
  driver.rms_current(600);    // Set stepper current to 600mA. The command is the same as command TMC2130.setCurrent(600, 0.11, 0.5);
  driver.stealthChop(1);      // Enable extremely quiet stepping
  driver.stealth_autoscale(1);
  driver.microsteps(microseps);

  stepper.setMaxSpeed(max_speed * steps_per_mm);
  stepper.setEnablePin(EN_PIN);
  stepper.setPinsInverted(false, false, true);
  stepper.enableOutputs();

  /* Homing */
  pinMode(STOP_PIN, INPUT);

  //set high acceleration for rapid breaking
  stepper.setAcceleration(1000 * steps_per_mm);

  Serial.println("Home...");

  stepper.move(-1500 * steps_per_mm);

  while (digitalRead(STOP_PIN) == 0) {
    stepper.run();
    yield(); //To reset Watchdog
  }

  stepper.move(10 * steps_per_mm);

  while (stepper.distanceToGo() != 0) {
    stepper.run();
    yield(); //To reset Watchdog
  }

  stepper.setCurrentPosition(0);
  stepper.setAcceleration(acceleration * steps_per_mm);

  Serial.println("Done!");
}

void loop() {
  server.handleClient();
  stepper.run();
  MDNS.update();

  if (stepper.distanceToGo() == 0) {
    if (stop) {
      stepper.disableOutputs();
      stop = false;
    }
    else if (bounce) {
      int pos = stepper.currentPosition() / steps_per_mm;
      int half = (pos_up + pos_down) / 2;

      stepper.moveTo((pos < half ? pos_up : pos_down) * steps_per_mm);
    }
  }
}
