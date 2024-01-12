// LIBRARIES //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// requires library: https://github.com/PaulStoffregen/Time
#include <TimeLib.h>    // can't use <Time.h> starting with v1.6.1

// requires library: https://github.com/PaulStoffregen/OneWire
// requires library: https://github.com/matmunk/DS18B20/
#include <DS18B20.h>
DS18B20 ds(A0);

// requires library: https://github.com/mcci-catena/arduino-lmic
// Remember to change on config.h the LoRa frequency to EU's 868MHz
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

// requires library: https://github.com/adafruit/Adafruit_SleepyDog
#include <Adafruit_SleepyDog.h>

// Standard C libraries
#include <string.h>
#include <stdint.h>

// PARAMETERS //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// This EUI must be in little-endian format, so least-significant-byte
// first. When copying an EUI from ttnctl output, this means to reverse
// the bytes. For TTN issued EUIs the last bytes should be 0xD5, 0xB3,
// 0x70.
static const u1_t PROGMEM APPEUI[8]={ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}

// This should also be in little endian format, see above.
//static const u1_t PROGMEM DEVEUI[8]={ 0xAB, 0x1C, 0x00, 0xD8, 0x7E, 0xD5, 0xB3, 0x70 }; // Hydrolab OTAA 1
static const u1_t PROGMEM DEVEUI[8]={ 0xAE, 0x24, 0x00, 0xD8, 0x7E, 0xD5, 0xB3, 0x70 }; // Hydrolab OTAA 2
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}

// This key should be in big endian format (or, since it is not really a
// number but a block of memory, endianness does not really apply). In
// practice, a key taken from ttnctl can be copied as-is.
static const u1_t PROGMEM APPKEY[16] = { FILL ME UP }; // FILL UP WITH THE PRIVATE APPKEY (not published to github obviously)
void os_getDevKey (u1_t* buf) {  memcpy_P(buf, APPKEY, 16);}

// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations).
const unsigned TX_INTERVAL = 60*2;//30;

// Pin mapping
const lmic_pinmap lmic_pins = {
    .nss = 10,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 5,
    .dio = {2, 3, LMIC_UNUSED_PIN},
};

#define LED_RED A4
#define LED_GREEN A5
#define LED_BLUE 4
#define ENA A3
#define PUMP 6

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static osjob_t sendjob;
uint32_t userUTCTime; // Seconds since the UTC epoch
static uint8_t payload[50];
int failed_attempts = 0;
bool first_message_sent = false;
bool time_updated = false;
bool first_downlink_received = false;
bool not_watered_yet = true;
bool immediate_uplink = false;
bool wd_reset_occurred = false;
bool interval_set = false;

struct {
  int humidity_percentage;
  int battery_percentage;
  float temperature_celcius;
  uint32_t hour_ranges;
  int humidity_threshold;
  int watering_time;
  int time_between_waterings;
  ostime_t last_watered_at;
  int minutes_between_tx;
  } sensor_status;

void do_send(osjob_t* j) {
    Serial.print("#");
    update_payload();
    Serial.print("Sending data: ");
    Serial.println((char *) payload);
    Serial.flush();

    interval_set = false;

    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        //Serial.println("Aborting. TX/RX running");
        return;
    }

    // Schedule a network time request at the next possible time
    LMIC_requestNetworkTime(user_request_network_time_callback, &userUTCTime);

    // Prepare upstream data transmission at the next possible time.
    LMIC_setTxData2(1, payload, strlen(payload), 0);
    //Serial.println(F("Packet queued for transmission"));
}

void update_payload() {
  sensor_status.temperature_celcius = ds.getTempC();
  sensor_status.humidity_percentage = humidity_sensor_percent();
  sensor_status.battery_percentage = battery_percent();

  char tmp_buffer[10];
  dtostrf(sensor_status.temperature_celcius, 5, 2, tmp_buffer);

  int minutes_since_last_water;
  if (not_watered_yet) {
    minutes_since_last_water = -1;
  }
  else {
    int32_t raw_minutes = (int32_t) (osticks2ms(os_getTime() - sensor_status.last_watered_at) / 1e3 / 60);
    minutes_since_last_water = raw_minutes > 9999 ? 9999 : raw_minutes;
  }

  sprintf(payload,
    "T%sH%iV%iL%iM%iN%i@%iS%i%s%s%sR%lxZ",
    tmp_buffer,
    sensor_status.humidity_percentage,
    sensor_status.battery_percentage,
    sensor_status.humidity_threshold,
    sensor_status.time_between_waterings,
    sensor_status.watering_time,
    minutes_since_last_water,
    sensor_status.minutes_between_tx,
    !wd_reset_occurred                        ? "" : "E1", // Error1
    first_message_sent                        ? "" : "E2", // Error2
    ((sensor_status.battery_percentage) > 20) ? "" : "E3", // Error3
    sensor_status.hour_ranges
  );

  wd_reset_occurred = false;
}

void process_downlink() {
  // payload contains the message
  char num_buffer[7];
  uint8_t num_buffer_length = 0;
  char mode;
  for (int i=0; payload[i]!='\0'; i++) {
    char current = payload[i];
    if ((current >= '0' && current <= '9') || (current >= 'a' && current <= 'f')) {
      num_buffer[num_buffer_length++]=current;
    }
    else {
      if (i!= 0) {
        num_buffer[num_buffer_length]='\0';
        process_individual_downlink_data(mode, num_buffer);
        num_buffer_length=0;
      }
      mode = current;
    }
  }

  digitalWrite(LED_BLUE, LOW);
  delay(1000);
  digitalWrite(LED_BLUE, HIGH);
  first_downlink_received = true;
}

void process_individual_downlink_data(char mode, char num_buffer[]) {
  switch(mode) {
     case 'R':
       sensor_status.hour_ranges = (int32_t) strtol(num_buffer, NULL, 16);
       break;
     case 'L':
       sensor_status.humidity_threshold = atoi(num_buffer);
       break;
     case 'M':
       sensor_status.time_between_waterings = atoi(num_buffer);
       break;
     case 'N':
       sensor_status.watering_time = atoi(num_buffer);
       break;
     case 'W':
       water(atoi(num_buffer));
       break;
     case 'S':
       sensor_status.minutes_between_tx = atoi(num_buffer);
       break;
     case 'Y':
       if (atoi(num_buffer)==1) {
         immediate_uplink = true;
       }
       break;
     default: break;
  }
}

static int get_tx_interval() {
  if (immediate_uplink) {
    immediate_uplink = false;
    return 1;
  }
  
  if (should_water_now()) { 
    return 60;
  }
  if (!first_downlink_received) return TX_INTERVAL;
  return 60*sensor_status.minutes_between_tx;
}

static void decide_on_my_own() {
  if (should_water_now())
      water(sensor_status.watering_time);
}

static bool should_water_now() {
  if (!first_downlink_received) return false;

  if ( sensor_status.humidity_percentage >= sensor_status.humidity_threshold ) return false;
  if ( (osticks2ms(os_getTime() - sensor_status.last_watered_at) / 1000) < sensor_status.time_between_waterings) return false;
  if ( (sensor_status.hour_ranges >> hour()) % 2 != 0 ) return true;

  return true;
}

static void water(int seconds) {
  digitalWrite(PUMP, HIGH);
  for(int i=seconds; i>0; i--) {
    delay(1000);
  }
  digitalWrite(PUMP, LOW);

  sensor_status.last_watered_at = os_getTime();
  not_watered_yet = false;
}

void setup() {
    pinMode(ENA, OUTPUT);
    digitalWrite(ENA, HIGH);

    pinMode(PUMP, OUTPUT);
    digitalWrite(PUMP, LOW);

    wd_reset_occurred = MCUSR & _BV(WDRF);
  
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);

    digitalWrite(LED_RED, HIGH); // Leds are on a pull-up so high means they're off.
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);

    digitalWrite(LED_RED, LOW);
    delay(1000);
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_BLUE, LOW);
    
    Serial.begin(9600);
    Serial.println(F("Starting"));

    // LMIC init
    //os_init();
    if (!os_init_ex((const void *)&lmic_pins)) {
      Serial.println(F("MISO_LORA disconnected"));
      digitalWrite(LED_BLUE, HIGH);
      while(true) {
        digitalWrite(LED_RED, LOW);
        delay(500);
        digitalWrite(LED_RED, HIGH);
        delay(500);
      }
    }
    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();

    battery_percent_init();
    humidity_sensor_init();

    digitalWrite(LED_BLUE, HIGH);
    digitalWrite(LED_GREEN, LOW);
    delay(500);
    digitalWrite(LED_GREEN, HIGH);

    // Start job (sending automatically starts OTAA too)
    do_send(&sendjob);
    Watchdog.enable(30000);
}

void loop() {
    os_runloop_once();
    Watchdog.reset();
}
