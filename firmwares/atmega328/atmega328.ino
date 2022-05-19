#include <SingleWireSerial.h>
SingleWireSerial front_keypad(false);
#include "LowPower.h"
#include <EEPROM.h>

/* Modes
 *  
 *  1 = static code
 *  2 = OTP
 *  3 = OTP or static code
 *  4 = static code and OTP
 */
const byte mode = 1;

// I/O defs
const int wakeUpPin = 2;
const int lock_pin = 6;
const int esp_pin = 3;

const int signal_offset = 2;//bytes
int code_length = 5;
byte request[9];
int request_length = 0;
bool change_code = false;
bool wait_otp = false;

// Anti bruteforce settings
const int max_wrong_code = 3;
const int reset_locking_timer = 60;//sec
long last_lock = 0;

// EEPROM address settings
const int eeprom_code_start_addr = 0;
const int wrong_code_count_addr = 100;

// Timers settings
const int open_timer = 3;//sec
const int sleep_timer = 3;//sec
const int change_timer = 10;//sec
const int wait_otp_timer = 15;//sec
long last_wake;

void setup() {
  Serial.begin(4800);
  front_keypad.begin(4800);
  
  pinMode(wakeUpPin, INPUT_PULLUP);
  pinMode(lock_pin, OUTPUT);
  digitalWrite(lock_pin, LOW);
  pinMode(esp_pin, OUTPUT);
  digitalWrite(esp_pin, LOW);
  
  enter_sleep_mode();
}

long seconds(){
  return (millis()/1000);
}

void clear_request_data(){
  request_length = 0;
  memset(request, 0, sizeof(request));
}

void enter_sleep_mode(){
  clear_request_data();
  change_code = false;
  wait_otp = false;

  attachInterrupt(0, wakeUp, LOW);
  delay(1000);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  detachInterrupt(0);
  last_wake = seconds();
}


void wakeUp(){}

void loop() {

  // Receive keypad messages
  if (front_keypad.available() > 0) {
    byte input = front_keypad.read();
   
    request[request_length] = input;
    request_length++;
    
    if(request_length >= sizeof(request)){
      switch(request[2]){
        case 113: try_unlock();break;
        case 116: ask_change();break;
        case 117: set_code();break;
        case 120: send_reply(0x58, 0x1, 0x0, 0x1);break;
        default: break;
      }
      clear_request_data();
    }
  }

  if(!is_locked()){// Do not sleep if locked, cause timer stop in sleep mode
    if((((seconds() - last_wake) > sleep_timer) && !change_code) || (((seconds() - last_wake) > change_timer) && change_code)){
      enter_sleep_mode();
    }
  }
}

bool check_code(){

  if(is_locked()){
    return false;
  }
  
  bool match = true;
  for(int i = 0;i < code_length;i++){
    if(EEPROM.read(eeprom_code_start_addr+i) != request[i+3]){
      match = false;
    }
  }

  if(!match){
    add_wrong_code();
  }
  
  return match;
}

bool check_otp(){

  digitalWrite(esp_pin, HIGH);
  delay(1000);
  Serial.readStringUntil('#');
  delay(100);
  while(Serial.available() == 0){
    delay(1);  
  }
  String otp = Serial.readStringUntil('\r');
  digitalWrite(esp_pin, LOW);
  
  bool match = true;
  for(int i = 0;i < code_length;i++){
    if(byte(otp[i]-48) != request[i+3]){
      match = false;
    }
  }
  return match;
}

void try_unlock(){

  if(wait_otp || mode == 2){
    send_reply(0x51, 0x1, 0x0, 0x1);
    if(check_otp()){
      open();
    }
    wait_otp = false;
    enter_sleep_mode();
  }else if(check_code()){
    if(mode == 4){
      send_reply(0x51, 0x1, 0x0, 0x1);
      wait_otp = true;
      last_wake += wait_otp_timer;
    }else{
      send_reply(0x51, 0x1, 0x0, 0x1);
      open();
    }
  }else if(mode == 3){
    send_reply(0x51, 0x1, 0x0, 0x1);
    if(check_otp()){
      open();
    }
  }
}

void ask_change(){
  if(check_code()){
    change_code = true;
    send_reply(0x54, 0x1, 0x0, 0x2);
  }
}

void set_code(){
  if(change_code){
    for(int i = 0;i < code_length;i++){
      EEPROM.write(eeprom_code_start_addr+i, request[i+3]);
    }
    change_code = false;
    send_reply(0x55, 0x1, 0x0, 0x2);
  }
}

bool is_locked(){
  bool locked = (EEPROM.read(wrong_code_count_addr) >= max_wrong_code);
  if(locked){
    if(!last_lock){
      last_lock = seconds();
    }
    if((seconds() - last_lock) >= reset_locking_timer){
      last_lock = 0;
      reset_wrong_code_count();
      return false;
    }
  }
  
  return locked;
}

void open(){
  digitalWrite(lock_pin, HIGH);
  delay(open_timer*1000);
  digitalWrite(lock_pin, LOW);
}

void add_wrong_code(){
  EEPROM.write(wrong_code_count_addr, EEPROM.read(wrong_code_count_addr)+1);
}

void reset_wrong_code_count(){
  EEPROM.write(wrong_code_count_addr, 0);
}

void send_reply(byte command, byte data_a, byte data_b, byte data_c){
  byte checksum = (command + data_a + data_b + data_c);
  front_keypad.write(0x0);
  front_keypad.write(command);
  front_keypad.write(data_a);
  front_keypad.write(data_b);
  front_keypad.write(data_c);
  front_keypad.write(checksum);
}
