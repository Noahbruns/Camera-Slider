/* Create a WiFi access point and provide a web server on it. */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "html.h"

/* Slider */
#define EN_PIN    5  //  D1 
#define DIR_PIN   4  //  D2    
#define STEP_PIN  0  //  D3  
#define CS_PIN    15 // D8   
#define MOSI_PIN  13 //  D7
#define MISO_PIN  12 //   D6
#define SCK_PIN   14 //   D5 

constexpr uint32_t microseps    = 256; 
constexpr float steps_per_mm_float = 6.83;
constexpr float max_speed = 30; // mm/s
constexpr float acceleration = 10; // mm/s²
constexpr uint32_t steps_per_mm = steps_per_mm_float * microseps;

constexpr uint32_t pos_up = 1100;
constexpr uint32_t pos_down = 0;


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
  stepper.moveTo(steps_per_mm * pos_up);

  server.send(200, "text/html", "OK");
}

void handlegoDown() {
  stepper.moveTo(steps_per_mm * pos_down);

  server.send(200, "text/html", "OK");
}

void handleSpeed() {
  if (server.hasArg("q")) {
    long speed = server.arg("q").toInt();
    stepper.setMaxSpeed(speed * steps_per_mm / 10);
  }

  server.send(200, "text/html", String(stepper.maxSpeed() * 10 / steps_per_mm));
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

  IPAddress myIP = WiFi.localIP();
  Serial.print("IP address: ");
  Serial.println(myIP);

  server.on("/", handleRoot);
  server.on("/val", handleVal);
  server.on("/goUp", handlegoUp);
  server.on("/goDown", handlegoDown);
  server.on("/speed", handleSpeed);

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
  stepper.setAcceleration(acceleration * steps_per_mm);
  stepper.setEnablePin(EN_PIN);
  stepper.setPinsInverted(false, false, true);
  stepper.setCurrentPosition(0);
  stepper.enableOutputs();
}

void loop() {
  server.handleClient();
  stepper.run();
}
