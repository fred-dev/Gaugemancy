import time
import board
import digitalio
import adafruit_lis3dh
from gpiozero import Button

button = Button(19)
print("Python setup: Button is set for pin 19");
i2c = board.I2C()
lis3dh = adafruit_lis3dh.LIS3DH_I2C(i2c)

# Set range of accelerometer (can be RANGE_2_G, RANGE_4_G, RANGE_8_G or RANGE_16_G).
lis3dh.range = adafruit_lis3dh.RANGE_2_G
print("Python setup: LIS3DH is set to range 2");

# Read accelerometer values (in m / s ^ 2).  Returns a 3-tuple of x, y,
# z axis values.  Divide them by 9.806 to convert to Gs.
x, y, z = [value / adafruit_lis3dh.STANDARD_GRAVITY for value in lis3dh.acceleration]
print("x = %0.3f G, y = %0.3f G, z = %0.3f G" % (x, y, z))
# Small delay to keep things responsive but give time for interrupt processing.

print("Python setup: Setup is done, exiting I hope");

