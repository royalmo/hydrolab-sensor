// This sketch uses Timer1 and PIN9 (PORTB1)

#define NUM_SAMPLES_HUMIDITY 3

// Run this in setup()
void humidity_sensor_init() {
  pinMode(9, OUTPUT);
  TCCR1A = (1<<COM1A0);
  TCCR1B = (1<<WGM12);
  OCR1A = 3; // 1/8 F_CPU -> 1MHz.
}

// This BLOCKING function will return a 0-100 integer
// rempresenting the relative humidity
int humidity_sensor_percent() {
  // Starting 1MHz wave in PIN9
  TCCR1B |= (1<<CS10);

  // Waiting some prudential time so transitory time has passed
  delay(2000);

  int raw = 0;
  for(int i = 0; i<NUM_SAMPLES_HUMIDITY; i++) {
    delay(500);
    raw += analogRead(A1);
  }

  // Stopping 1MHz wave
  TCCR1B = (1<<WGM12);

  int humidity_percent;
  if (raw < 450*NUM_SAMPLES_HUMIDITY)
    humidity_percent = 100;
  else if (raw > 800*NUM_SAMPLES_HUMIDITY)
    humidity_percent = 0;
  else
    humidity_percent = map(raw, 450*NUM_SAMPLES_HUMIDITY, 800*NUM_SAMPLES_HUMIDITY, 100, 0);

  return humidity_percent;
}
