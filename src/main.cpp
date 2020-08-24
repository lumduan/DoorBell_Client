// Thanks Santi&be  > Youtube
// THANKS dancol90 @github

#include <WiFi.h>
#include <esp_camera.h>
#include <esp_system.h>
#include <TridentTD_LineNotify.h>
#include <ESP32Ping.h> 
#include <time.h>

#define SSID        "SSID"   // Wifi ที่จะให้ใช้เชื่อม
#define PASSWORD    "SSID_PASSWORD"   //PASSWORD

#define LINE_TOKEN  "LINE_TOKEN"  // ใส่ Line Token หากยังไม่มีเข้าไปสร้างที่ https://notify-bot.line.me/th/
#define LINE_MESSAGE  "มีผู้มาติดต่อ @ประตูร้านกาแฟ"    // ข้อความที่ส่งผ่าน Line Noti

#define CLIENT_ID   '0'  // ID Cline โดยจะส่งค่านี้ไปที่ Speaker

#define PORT 8888 // Port สำหรับเชื่อมต่อ Speaker


// ใส่ Speaker ที่ต้องการให้ส่งข้อมูล จะให้ส่งข้อมูลไปให้กี่ตัวก็สามารถใส่เลข IP ต่อลงไปได้เลย
IPAddress SERVER_IP[] = {{192,168,1,202}, //  Speaker ชั้น 2
                          {192,168,1,203} // Speaker ชั้น 3
                          }; // 

IPAddress GATEWAY_IP = {192,168,1,1}; // Gateway ใช้เป็น IP Router ก็ได้ หาก Ping ไม่เจอจะทำการ Reboot ระบบ เพื่อเชื่อมต่อใหม่อีกครั้ง

WiFiClient client; 

const int TRIGGER_DISTANCE =  10; // ระยะตรวจจับ cm (เมื่อต่ำกว่า) ระบบจะทำงาน

const bool FLASH_ON = false; // ให้เปิด Flash หรือไม่ true / false

bool FIRST_RUN = true; // ทดสอบระบบ เมื่อเริ่มทำงานครั้งแรก

//////////////////// ส่วนของกล้อง ESP32CAM ////////////////////
// PIN ที่ใช้งานกล้อง CAMERA_MODEL_AI_THINKER 
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

const int FLASH = 4; // Pin CAM FLASH

/////////////////////////////////////////

// Pin ที่ใช้เชื่อมต่อ Ultrasonic โมดูล Trig และ Echo
const int U_SONIC_TRIG = 12; 
const int U_SONIC_ECHO = 13;

// Pin ที่ใช้เชื่อมต่อ BUZZER โมดูล
const int BUZZER = 14; 


// ************** Function ในโปรแกรม **************** //

void Send_line(uint8_t *image_data,size_t   image_size); // FN ส่ง Line Messenger

void connectServer(); // FN ต่อกับ Speaker

void Camera_capture(); // FN ถ่ายภาพ

void Beep(); // เสียง Beep 

int distanceCal(); // หาค่าระยะทาง

void checkGateWay(); // เช็ค ออนไลน์ อยู่ไหม

// ************** END of Functions **************** //




void setup() {

  Serial.begin(9600);

  pinMode(U_SONIC_TRIG,OUTPUT); // pin 12 OUTPUT
  pinMode(U_SONIC_ECHO,INPUT); // pin 13 INPUT
  pinMode(BUZZER,OUTPUT); // pin 14 OUTPUT
  pinMode(FLASH, OUTPUT); // pin 4 OUTPUT

  digitalWrite(BUZZER,HIGH);

  while (!Serial) {  ;  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);

  Serial.printf("WiFi connecting to %s\n",  SSID);

  while(WiFi.status() != WL_CONNECTED) { Serial.print("."); delay(400); }
  Serial.printf("\nWiFi connected\nIP : ");
  Serial.println(WiFi.localIP());
  
  LINE.setToken(LINE_TOKEN);

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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; 
  
  if(psramFound()){

    config.frame_size = FRAMESIZE_UXGA; 
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_XGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }


  if(FIRST_RUN){

    Serial.println("First Running ...");

    FIRST_RUN = false;
    
    Beep();

    connectServer();
    
    Camera_capture();

  }

}


void loop() {
    

    Serial.println(distanceCal()); // แสดงระยะ (cm)

    if(TRIGGER_DISTANCE > distanceCal()){
      delay(300); //ดีเลย์ ครึ่งวิ เพื่อป้องกันการ ERROR ของ Sensor และให้ตรวจจับอีกครั้งหนึ่ง หากมือยังอยู่่ที่เดิมให้ทำงาน

      if(TRIGGER_DISTANCE > distanceCal()){

        Beep();

        connectServer();

        Camera_capture();
        
      }
      
    }
    
}



/////////// FUNCTIONS //////////////

// FN ส่ง ข้อความและภาพผ่าน Line
void Send_line(uint8_t *image_data,size_t   image_size){
   LINE.notifyPicture(LINE_MESSAGE,image_data, image_size);
  }

// FN คำนวณระยะทางจาก Sensor Ultrasonic
int distanceCal(){
  int duration;

  digitalWrite(U_SONIC_TRIG, LOW);
  delayMicroseconds(2);
 
  digitalWrite(U_SONIC_TRIG, HIGH);
  delayMicroseconds(10);

  digitalWrite(U_SONIC_TRIG, LOW);
 
  duration = pulseIn(U_SONIC_ECHO, HIGH);
 
  return duration * 0.034/2;
}

// FN เชื่อมต่อ Speaker
void connectServer(){

  checkGateWay();

  for(int i = 0; i<sizeof(SERVER_IP)/sizeof(SERVER_IP[0]); i++) { // Loop จนเท่าจำนวน Array ของ Speaker

      Serial.print("Connecting server :  ");
      Serial.print(i+1);
      Serial.print(" of  ");
      Serial.println(sizeof(SERVER_IP)/sizeof(SERVER_IP[0]));

    if(Ping.ping(SERVER_IP[i])){ // ให้ Ping ตรวจสอบก่อนว่า Speaker Online หรือไม่

            client.connect(SERVER_IP[i],PORT);

            Serial.println("Server Connected");

            client.write(CLIENT_ID); // ส่งค่า ID ของ Client ไปให้ Speaker  

            client.write('\n');

            client.stop();
    }
    
    else{ // ถ้า Ping  Server ไม่เจอ 
          Serial.print("Can not connect server ");
          Serial.println(i+1);
    }

  }
}


// FN Capture
void Camera_capture() {

  if(FLASH_ON){
    digitalWrite(FLASH, HIGH);
    delay(100); 
    digitalWrite(FLASH, LOW);
    delay(100);
    digitalWrite(FLASH, HIGH);
  }

  camera_fb_t * fb = NULL;

  delay(200); 

  // Take Picture with Camera
  fb = esp_camera_fb_get(); 
  if(!fb) {
    Serial.println("Camera capture failed");
    return;
  }
    
    digitalWrite(FLASH, LOW);

    Send_line(fb->buf,fb->len);

    esp_camera_fb_return(fb); 

}

void Beep(){
  digitalWrite(BUZZER,LOW);
  delay(100);
  digitalWrite(BUZZER,HIGH);
  delay(100);
  digitalWrite(BUZZER,LOW);
  delay(100);
  digitalWrite(BUZZER,HIGH);
}

void checkGateWay(){

  if(!Ping.ping(GATEWAY_IP)){

    Serial.println("Can't Connect Gateway");
    Serial.println("ESP Restart .....");

    ESP.restart();

  }
  

}
