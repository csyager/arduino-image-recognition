#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>
#include <ArduCAM.h>
#include <Servo.h>
#include "memorysaver.h"

//------------------------ configure EthernetClient to send requests to cloudformation -----------------
byte mac[] = { 0xA8, 0x61, 0x0A, 0xAE, 0x81, 0xF8 };

EthernetClient client;

int HTTP_PORT = 80;
String HTTP_METHOD = "POST";
char HOST_NAME[] = "d1mudw2wp7jo4m.cloudfront.net";

//--------------------------------------- configure arducam --------------------------------------------
const int CS = 7;   // sets pin 7 as slave select for cam module's spi port
bool is_header = false;
int mode = 0;
uint8_t start_capture = 0;
#if defined (OV2640_MINI_2MP_PLUS)
ArduCAM myCAM( OV2640, CS );
#endif
uint8_t read_fifo_burst(ArduCAM myCAM);

//--------------------------------------- configure servo ----------------------------------------------
int servoPin = 5;
Servo servo;

//---------------------------------------------setup()--------------------------------------------------
void setup() {
  servo.attach(servoPin);
  uint8_t vid, pid;
  uint8_t temp;

  Wire.begin();
  Serial.begin(9600);
  Serial.println(F("Running..."));
  Serial.println(F("ArduCAM start"));

  pinMode(CS, OUTPUT);
  digitalWrite(CS, HIGH);
  SPI.begin();
  //Reset the CPLD
  myCAM.write_reg(0x07, 0x80);
  delay(100);
  myCAM.write_reg(0x07, 0x00);
  delay(100);

  while(1) {
    //Check if the ArduCAM SPI bus is OK
    myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    temp = myCAM.read_reg(ARDUCHIP_TEST1);
    if (temp != 0x55) {
      Serial.println(F("ACK CMD SPI interface Error!END"));
      delay(1000); continue;
    } else {
      Serial.println(F("ACK CMD SPI interface OK.END")); break;
    }
  }

#if defined (OV2640_MINI_2MP_PLUS)
  while (1) {
    //Check if the camera module type is OV2640
    myCAM.wrSensorReg8_8(0xff, 0x01);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
    if ((vid != 0x26 ) && (( pid != 0x41 ) || ( pid != 0x42 ))) {
      Serial.println(F("ACK CMD Can't find OV2640 module!"));
      delay(1000); continue;
    }
    else {
      Serial.println(F("ACK CMD OV2640 detected.END")); break;
    }
  }
#endif
  //Change to JPEG capture mode and initialize the OV5642 module
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  delay(1000);
  myCAM.clear_fifo_flag();

  if (Ethernet.begin(mac) == 0) {
    Serial.println(F("Failed to obtain an IP address using DHCP"));
    while(true);
  }


  // -------------------------------------------- execution ----------------------------------------
  temp = 0xff;
  uint8_t temp_last = 0;
  uint32_t len;
  bool is_header = false, errorflag = false;

  // Capture image from arducam
  myCAM.OV2640_set_JPEG_size(OV2640_800x600);
  delay(1000);
  Serial.println(F("ACK CMD switch to OV2640_800x600"));
  temp = 0xff;
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  myCAM.start_capture();
  start_capture = 0;
  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK));
  len = myCAM.read_fifo_length();
  Serial.println(len);
  if ((len >= MAX_FIFO_SIZE) | (len == 0)) {
    myCAM.clear_fifo_flag();
    Serial.println(F("ERR wrong size"));
    errorflag=1;
  }
  Serial.println(F("ACK CMD CAM Capture Done."));

//------------------------------------------------http request -------------------------------------
  if(!client.connect(HOST_NAME, HTTP_PORT)) {
    Serial.println(F("ERR http connection failed"));
    errorflag=1;
  }

  if (!errorflag) {
    myCAM.CS_LOW();
    myCAM.set_fifo_burst();
    SPI.transfer(0xFF);
    myCAM.CS_HIGH();
    Serial.println(F("Connected to the server"));
    String response = "POST /facial-rekognition HTTP/1.1\r\n";
    response += "Host: d1mudw2wp7jo4m.cloudfront.net\r\n";
    response += "Content-Type: image/jpeg\r\n";
    response += "Content-Length: " + String(len) + "\r\n";
    client.println(response); 

    static const size_t bufferSize = 512;
    static uint8_t buffer[bufferSize] = {0xFF};
    while (len) {
      size_t will_copy = (len < bufferSize) ? len: bufferSize;
      myCAM.CS_LOW();
      myCAM.set_fifo_burst();
      for (int bufptr=0; bufptr<will_copy; bufptr++){
        buffer[bufptr]=SPI.transfer(0x00);
      }
      myCAM.CS_HIGH();
      if (client.connected())
        client.write(&buffer[0], will_copy);
      len -= will_copy;
    }
    myCAM.CS_HIGH();
    int connectLoop=0;
    int index = 0;
    char statusCode[3];
    while(client.connected()) {
      while(client.available()) {
        char c = client.read();
        Serial.print(c);
        if(index >=9 && index <=11){
          statusCode[index-9] = c;
        }
        index += 1;
        connectLoop = 0;
      }
      connectLoop++;
      if (connectLoop > 5000){     // more than 1000 miliseconds since last packet
        Serial.println();
        client.stop();
      }
      delay(1);
    }
    Serial.println(statusCode);
    if ((String) statusCode == "200") {
        Serial.println("Moving servo");
        servo.write(0);
        delay(5000);
        servo.write(90);
    }
  }
  Serial.println(F("Execution loop terminated."));
}

void loop() {
  // put your main code here, to run repeatedly:
  

}
