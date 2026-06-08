#include "esp_camera.h"
#include <WiFi.h>

// ===================
// Select camera model
// ===================
#define CAMERA_MODEL_AI_THINKER // This is the most common ESP-CAM module

#include "camera_pins.h"

// ===========================
// Wi-Fi Station Configuration
// ===========================
const char* ssid = "elquesabesabe";
const char* password = "yelquenovaalauade";

// ==============================
// Wi-Fi Access Point (Fallback)
// ==============================
const char* ap_ssid = "ESPCAM-Fallback";
const char* ap_password = "password123"; // Must be at least 8 characters

void startCameraServer();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  // for larger pre-allocated frame buffer.
  if(psramFound()){
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    // Limit the frame size when PSRAM is not available
    config.frame_size = FRAMESIZE_SVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Sensor setup
  sensor_t * s = esp_camera_sensor_get();
  // Drop down frame size for higher initial framerate
  if(s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, -3);  // Lowest brightness (range: -2 to 2)
    s->set_saturation(s, 4);   // Maximum saturation (range: -2 to 2)
  }
  s->set_framesize(s, FRAMESIZE_QVGA);

  // ===========================
  // Wi-Fi Connection Logic
  // ===========================
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  Serial.print("Attempting to connect to Wi-Fi");
  int attempts = 0;
  
  // Try to connect for ~10 seconds
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    // Connection Successful
    Serial.println("Wi-Fi connected successfully!");
    Serial.print("Camera Stream Ready! Go to: http://");
    Serial.println(WiFi.localIP());
  } else {
    // Connection Failed -> Fallback to AP
    Serial.println("Wi-Fi connection failed. Falling back to Access Point mode.");
    
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
    
    Serial.println("AP Mode started.");
    Serial.print("Connect your device to Wi-Fi network: ");
    Serial.println(ap_ssid);
    Serial.print("Then go to: http://");
    Serial.println(WiFi.softAPIP());
  }

  // Start the web server (defined in app_httpd.cpp)
  startCameraServer();
}

void loop() {
  // Do nothing. The web server runs asynchronously.
  delay(10000);
}