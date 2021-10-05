# bike_cadence_nrf
Bicycle cadence sensor based on Nrf52 sdk

## used components

### mpu6050

Accelerometer with a gyroscope that works with 3.3V. Cheap, commonly available on eBay. 

#### cheap ebay board catch

The mpu6050 is capable of running the accelerometer in 1.25 Hz mode in a very low power mode that should consume couple of μA. The problem is that the cheap eBay boards contain both a voltage regulator, so it can be powered either with 5V or 3.3V and an LED. Both of which are consuming current in an order of mA. Since we are running relatively stable voltage from a battery, we can just remove the voltage regulator and short the pins. LED can just be removed.

### j-link programmer

### ebyte E73 (2G4M04S1A)

Cheapest reasonable board with nRF52 series processor (from Nordic Semiconductor) some I/O and Bluetooth antenna that I could find. The ebyte E73 contains nRF52810 that has a ARM® Cortex™-M4 CPU, supports Bluetooth 5, Bluetooth Low Energy and ANT. Which is way more than needed for this.

## how it works

The program has two states. Idle state and measurement. At the beginning we initialize everything, and set the mpu 6050 to a low power mode with only the accelerometer running. Then a timer reads the current accelerometer value for X,Y,Z axis every two seconds and compares it to the previous value. If it's different by more than a certain value, that was chosen with a very scientific method of picking a threshold that seemed about right, we configure the mpu6050 to stop the accelerometer and turn on the gyroscope.

Then we start a new timer that reads the gyro values every 10 ms, which is more that fast enough for this use. The gyro values are signed 16 bit integers for each axis that we can convert to degrees per second. Our setting is +-1000 degrees per second, so plus or minus 32768 is plus or minus 1000 degrees per second. After we get the converted value in degrees per second we calculate how much did it move in the 10 milliseconds since we read it last and increment the degree counter. After it counts the full turn of 360 degrees it increments the cadence counter, triggers the Bluetooth event and so on. 

When there is no 360 degree turn in two minutes, we stop the gyro measurements, turn off Bluetooth and start the accelerometer measurements - so back to idle mode.
