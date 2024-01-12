#define NUM_SAMPLES_BATTERY 5

// Run this in setup()
void battery_percent_init() {
  pinMode(7, INPUT);
}

// This BLOCKING function will return a 0-100 integer
// rempresenting the battery charge percentage
int battery_percent() {
  // Starting battery measurement
  pinMode(7, OUTPUT);
  digitalWrite(7, LOW);
  
  int raw = 0;
  for(int i = 0; i<NUM_SAMPLES_BATTERY; i++) {
    delay(300);
    raw += analogRead(A2);
  }

  // Stopping battery measurement
  pinMode(7, INPUT);

  // Getting average, converting to volts and "removing" the voltage divider.
  // Average voltage is Bat_Raw.
  // 100 = 1V
  int raw_voltage = (int) (((float) raw/NUM_SAMPLES_BATTERY)*(3.3/1024)*((float) 43/10)*100);

  // Now we need to implement the discharge curve. It's a very complicated function
  // and depends on a lot of factors. We think that it could be better and faster
  // to check it manually.

  // See chart at https://siliconlightworks.com/image/data/Info_Pages/Li-ion%20Discharge%20Voltage%20Curve%20Typical.jpg
  int battery_percent;

  if      (raw_voltage > 400)
    battery_percent = 100;
  else if (raw_voltage > 390)
    battery_percent = map(raw_voltage, 390, 400, 85, 95);
  else if (raw_voltage > 380)
    battery_percent = map(raw_voltage, 380, 390, 75, 85);
  else if (raw_voltage > 375)
    battery_percent = map(raw_voltage, 375, 380, 60, 75);
  else if (raw_voltage > 370)
    battery_percent = map(raw_voltage, 370, 375, 45, 60);
  else if (raw_voltage > 365)
    battery_percent = map(raw_voltage, 365, 370, 35, 45);
  else if (raw_voltage > 360)
    battery_percent = map(raw_voltage, 360, 365, 30, 35);
  else if (raw_voltage > 355)
    battery_percent = map(raw_voltage, 355, 360, 10, 30);
  else if (raw_voltage > 325)
    battery_percent = map(raw_voltage, 325, 355, 0,  10);
  else
    battery_percent = 0;

  return battery_percent;
}
