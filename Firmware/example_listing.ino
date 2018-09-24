#include <SPI.h>
#include <Wire.h>

//#define DEBUG

// Pin definitions
#define STAT 3 // status LED
#define BTN 4  // pushbutton

// SPI pins (only these support hardware SPI!)
#define SS  10  // Slave Select
#define MOSI 11 // Master Out Slave In
#define SCK 13  // Serial ClocK

// I2C pins are initialised by Wire.h
// SDA: analog 4
// SCL: analog 5

// Max7219 registers
#define REG_NOOP           0x00
//REG_COLUMN:       0x01 - 0x08
#define REG_DECODEMODE     0x09
#define REG_INTENSITY      0x0a
#define REG_SCANLIMIT      0x0b
#define REG_SHUTDOWN       0x0c
#define REG_DISPLAYTEST    0x0f

// EEPROM address
#define DISK               0x50
#define INS_LEN            16

// Mtp led columns are wired to the DIG pins via a common cathode (-)
// Mtp led rows are wired to the SEG pins via a common anode (+)

// Lighting instruction data structure (directly accessible as byte array)
typedef struct __attribute__((packed)) ins{
    // 2 bytes: jump address BUT: 4 least significant bits store the brightness
    // 2 bytes: number of repeats for the jump
    // 4 bytes: duration in ms (unsigned long)
    // 8 bytes: LED matrix (column by column)
    uint16_t jump;
    uint16_t repeats;
    uint32_t duration;
    byte leds[8];
  } INS; // exactly 16 bytes, this must be enforced
// Example: INS instruction = {0 + 0xB, 2*1000, 10, {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}};
// Pointer to raw byte array can be passed like this: (byte *) &instruction

// Debugging functions:
#ifdef DEBUG
void print_instruction(struct ins *i)
{
  char buf[128];
  sprintf(buf, "intensity: %2d; duration: %lu ms; jump to %u x %u times; LEDs: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
    (i->jump & 0x0F), i->duration, (i->jump & 0xFFF0)/INS_LEN, i->repeats,
    i->leds[0], i->leds[1], i->leds[2], i->leds[3], i->leds[4], i->leds[5], i->leds[6], i->leds[7]);
  Serial.println(buf);
}
int get_free_memory()
{
  // Credit: https://learn.adafruit.com/memories-of-an-arduino/measuring-free-memory
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
#endif

// MAX7219 LED driver functions:
void send_cmd(byte address, byte value){
  // Take the SS pin low to select the chip:
  digitalWrite(SS,LOW);
  // Send in the address and value via SPI:
  SPI.transfer(address);
  SPI.transfer(value);
  // Take the SS pin high to de-select the chip:
  digitalWrite(SS,HIGH);
}
void max7219_init() {
  send_cmd(REG_SCANLIMIT, 0x07);
  send_cmd(REG_DECODEMODE, 0x00);  // Using an led matrix (not digits)
  send_cmd(REG_SHUTDOWN, 0x01);    // Not in shutdown mode
  send_cmd(REG_DISPLAYTEST, 0x00); // No display test
  for (int c=1; c<=8; c++) {    // Turn all LED off
    send_cmd(c,0);
  }
  send_cmd(REG_INTENSITY, 0x0f & 0x0f); // 0x0f is used as a mask since you can only set 16 levels
}
void clear_display(){
  for (uint8_t c=1; c<=8; c++){ send_cmd(c, 0);}
}

// 24LC256 EEPROM functions:
void write_bytes(uint16_t address, byte *source)
{
  // This does NOT take into account page boundaries
  //  (pages are 64 bytes wide)
  Wire.beginTransmission(DISK);
  Wire.write((byte)(address >> 8));   // MSB
  Wire.write((byte)(address & 0xFF)); // LSB
  for (uint8_t i = 0; i < INS_LEN; i++){
    Wire.write((byte) source[i]);
  }
  Wire.endTransmission();
  delay(6); // Give EEPROM IC time to write
}
void read_bytes(uint16_t address, byte *destination)
{
  // Read instruction from address as a byte array
  Wire.beginTransmission(DISK);
  Wire.write((byte)(address >> 8));   // MSB
  Wire.write((byte)(address & 0xF0)); // LSB, discarding 4 least significant bits
  Wire.endTransmission();
  Wire.requestFrom(DISK,INS_LEN);
  uint8_t i = 0;
  while(Wire.available()) destination[i++] = Wire.read(); // Should not overflow (destination length == INS_LEN)
}

// Lighting program interpreter:
void parse_protocol(uint16_t address, uint16_t endAddress)
{
  INS *instruction;
  instruction = (ins *) malloc(INS_LEN); // Remember to free() later
  for(;;){
    read_bytes(address, (byte *) instruction);
#ifdef DEBUG
    Serial.print("Program line "); Serial.println(address/INS_LEN);
    print_instruction(instruction);
    Serial.print("Free memory: "); Serial.println(get_free_memory());
#endif
    // Light LEDS
    send_cmd(REG_INTENSITY, (byte) instruction->jump & 0x0F); // brightness is encoded the pointer to the next instruction
    for(uint8_t c=0; c<8; c++){ send_cmd(c+1, instruction->leds[c]); }

    // Wait
    delay(instruction->duration);

    // Magic number to indicate final instruction
    if(instruction->jump == 0xFFFF){break;}

    // Iteration with recursion for the jumps
    if(address == endAddress){break;}
    for(uint16_t i=1; i<=instruction->repeats; i++){
#ifdef DEBUG
      Serial.print("Executing jump ");Serial.print(i);Serial.print(" of ");Serial.print(instruction->repeats);
      Serial.print(" to program line ");Serial.println((instruction->jump & 0xF0)/INS_LEN );
#endif
      parse_protocol(instruction->jump & 0xFFF0, address);
    }
    address += INS_LEN;
  }
  free(instruction); // Otherwise we get a memory leak
}

// Overloaded function definition to allow for optional arguments
void parse_protocol()
{
  parse_protocol(0x0000, 0xFFFF);
}

void setup() {

#ifdef DEBUG
    // Serial console
    delay(1000); // Give time for serial monitor to come up
    Serial.begin(9600);
    Serial.print("Initialised...\n");
    Serial.print("Free memory: "); Serial.println(get_free_memory());
#endif
    //  Setup pins
    pinMode(STAT, OUTPUT); // Status LED
    pinMode(BTN, INPUT); // Pushbutton
    pinMode(SS, OUTPUT);
    pinMode(MOSI, OUTPUT);
    pinMode(SCK, OUTPUT);
    //  Initialize SPI parameters
    SPI.begin();
    SPI.setBitOrder(MSBFIRST);  //  Most significant bit is sent first
    SPI.setDataMode(SPI_MODE3); //  Data is latched into device on rising edge
    SPI.setClockDivider(SPI_CLOCK_DIV4); //  Clock is 16MHz/4 = 4Mhz < 10Mhz max device speed
    // Initialize I2C bus (pins hardcoded in library)
    Wire.begin();

    // Initialize Max7219 LED driver chip
    max7219_init();
    clear_display();

    // Define program (harcoded example)
    INS prog[] = {{0 | 0x01, 0U, 3600000UL, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }}, // -1h
                  {0 | 0x01, 0U, 3600000UL, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF }}, // 0h
                  {0 | 0x01, 0U, 3600000UL, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF }}, // 1h
                  {0 | 0x01, 0U, 3600000UL, { 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF }}, // 2h
                  {0 | 0x01, 0U, 3600000UL, { 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF }}, // 3h
                  {0 | 0x01, 0U, 3600000UL, { 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }}, // 4h
                  {0 | 0x01, 0U, 1800000UL, { 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }}, // 5h
                  {0 | 0x01, 0U,  900000UL, { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }}, // 5h30
                  {0 | 0x01, 0U,  900000UL, { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }}, // 5h45
                  {0 | 0x01, 0U, 3600000UL, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }}};// 6h

    for(uint8_t i = 0; i < sizeof(prog)/INS_LEN; i++){
      write_bytes(i*INS_LEN,(byte *) &prog[i]);
    }
#ifdef DEBUG
  Serial.print("Free memory: "); Serial.println(get_free_memory());
#endif
}

void loop() {
  parse_protocol();
}
