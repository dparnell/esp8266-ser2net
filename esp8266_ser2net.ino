/*
  ESP8266 mDNS serial wifi bridge by Daniel Parnell 2nd of May 2015
 */

//#define BONJOUR_SUPPORT
#define USE_WDT

#include <ESP8266WiFi.h>
#ifdef BONJOUR_SUPPORT
#include <ESP8266mDNS.h>
#endif
#include <WiFiClient.h>

#include "esp8266_pwm.h"

// application config

// comment out the following line to enable DHCP
#define STATIC_IP

#ifdef STATIC_IP
// change the IP address and gateway to match your network
#define IP_ADDRESS "192.168.10.42"
#define GATEWAY_ADDRESS "192.168.10.1"
#define NET_MASK "255.255.255.0"
#endif

// change the network details to match your wifi network
#define WIFI_SSID "my network"
#define WIFI_PASSWORD "supersecret password nobody could ever guess"

#define BAUD_RATE 9600
#define TCP_LISTEN_PORT 9999

// if the bonjour support is turned on, then use the following as the name
#define DEVICE_NAME "ser2net"

// serial end ethernet buffer size
#define BUFFER_SIZE 128

// hardware config
#define WIFI_LED 14
//#define CONNECTION_LED 16
#define TX_LED 12
#define RX_LED 13

#ifdef BONJOUR_SUPPORT
// multicast DNS responder
MDNSResponder mdns;
#endif

WiFiServer server(TCP_LISTEN_PORT);
ESP8266_PWM pwm;

#ifdef STATIC_IP
IPAddress parse_ip_address(const char *str) {
    IPAddress result;    
    int index = 0;

    result[0] = 0;
    while (*str) {
        if (isdigit((unsigned char)*str)) {
            result[index] *= 10;
            result[index] += *str - '0';
        } else {
            index++;
            if(index<4) {
              result[index] = 0;
            }
        }
        str++;
    }
    
    return result;
}

#endif

void connect_to_wifi() {
  int count = 0;
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

#ifdef STATIC_IP
  IPAddress ip_address = parse_ip_address(IP_ADDRESS);
  IPAddress gateway_address = parse_ip_address(GATEWAY_ADDRESS);
  IPAddress netmask = parse_ip_address(NET_MASK);
  
  WiFi.config(ip_address, gateway_address, netmask);
#endif
  
#ifdef CONNECTION_LED
  digitalWrite(CONNECTION_LED, LOW);
#endif
  digitalWrite(TX_LED, LOW);
  digitalWrite(RX_LED, LOW);
  
  // Wait for WIFI connection
  while (WiFi.status() != WL_CONNECTED) {
#ifdef USE_WDT
    wdt_reset();
#endif
    pwm.set(0, (count & 1) ? PWM_MAX_DUTY : 0);
    count++;
    delay(250);
  }
  
  pwm.set(0, PWM_MAX_DUTY);  
}

void error() {
  int count = 0;
  
#ifdef CONNECTION_LED  
  digitalWrite(CONNECTION_LED, LOW);
#endif
  digitalWrite(TX_LED, LOW);
  digitalWrite(RX_LED, LOW);
  
  while(1) {
    count++;
    pwm.set(0, (count & 1) ? PWM_MAX_DUTY : 0);
    delay(100);
  }
}

void setup(void)
{  
#ifdef USE_WDT
  wdt_enable(1000);
#endif

  digitalWrite(WIFI_LED, LOW);
#ifdef CONNECTION_LED  
  digitalWrite(CONNECTION_LED, LOW);
#endif
  digitalWrite(TX_LED, LOW);
  digitalWrite(RX_LED, LOW);
  
  pinMode(WIFI_LED, OUTPUT);
#ifdef CONNECTION_LED  
  pinMode(CONNECTION_LED, OUTPUT);
#endif
  pinMode(TX_LED, OUTPUT);
  pinMode(RX_LED, OUTPUT);

  // set up the Wifi LED for PWM
  pwm.connect(0, WIFI_LED);
  pwm.set(0, 0);
  pwm.begin(1, 500);
  
  Serial.begin(BAUD_RATE);
  
  // Connect to WiFi network
  connect_to_wifi();

#ifdef BONJOUR_SUPPORT
  // Set up mDNS responder:
  if (!mdns.begin(DEVICE_NAME, WiFi.localIP())) {
    error();
  }
#endif

  // Start TCP server
  server.begin();
}

WiFiClient client;
uint8 pulse = 0;
uint8 pulse_dir = 1;
int pulse_counter = 1;

void loop(void)
{
  size_t bytes_read;
  uint8_t net_buf[BUFFER_SIZE];
  uint8_t serial_buf[BUFFER_SIZE];
  
#ifdef USE_WDT
  wdt_reset();
#endif

  if(WiFi.status() != WL_CONNECTED) {
    // we've lost the connection, so we need to reconnect
    if(client) {
      client.stop();
    }
    connect_to_wifi();
  }
  
  pulse_counter--;
  if(pulse_counter == 0) {
    pulse_counter = 1000;
    if(pulse_dir) {
      pulse++;
      if(pulse == PWM_MAX_DUTY) {
        pulse_dir = 0;
      }
    } else {
      pulse--;
      if(pulse == 0) {
        pulse_dir = 1;
      }
    }
  
    pwm.set(0, pulse);
  }
  
#ifdef BONJOUR_SUPPORT
  // Check for any mDNS queries and send responses
  mdns.update();
#endif

  // Check if a client has connected
  if (!client) {
    // eat any bytes in the serial buffer as there is nothing to see them
    while(Serial.available()) {
      Serial.read();
    }
      
    client = server.available();
    if(!client) {      
#ifdef CONNECTION_LED  
      digitalWrite(CONNECTION_LED, LOW);
#endif      
      return;
    }
    
#ifdef CONNECTION_LED  
    digitalWrite(CONNECTION_LED, HIGH);
#endif
  }

  if(client.connected()) {
    // check the network for any bytes to send to the serial
    int count = client.available();
    if(count > 0) {      
      digitalWrite(TX_LED, HIGH);
      
      bytes_read = client.read(net_buf, min(count, BUFFER_SIZE));
      Serial.write(net_buf, bytes_read);
      Serial.flush();
    } else {
      digitalWrite(TX_LED, LOW);
    }
    
    // now check the serial for any bytes to send to the network
    bytes_read = 0;
    while(Serial.available() && bytes_read < BUFFER_SIZE) {
      serial_buf[bytes_read] = Serial.read();
      bytes_read++;
    }
    
    if(bytes_read > 0) {  
      digitalWrite(RX_LED, HIGH);
      client.write((const uint8_t*)serial_buf, bytes_read);
      client.flush();
    } else {
      digitalWrite(RX_LED, LOW);
    }
  } else {
    // make sure the TX and RX LEDs aren't on
    digitalWrite(TX_LED, LOW);
    digitalWrite(RX_LED, LOW);
#ifdef CONNECTION_LED  
    digitalWrite(CONNECTION_LED, LOW);
#endif

    client.stop();
  }
}

