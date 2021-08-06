# DFRobotIRPositionEx Arduino library
A modified version of the DFRobotIRPosition library with extended functionality. This is for use with DFRobot's IR Positioning camera  (SEN0158). Much of the extended functionality comes from the WiiBrew wiki: http://wiibrew.org/wiki/Wiimote#IR_Camera

## New features
- Added Basic data format which has less IIC bytes than Extended format.
- Added unpack object size data from the Extended data format.
- added object seen flags to know when positions update and are visible or aren't seen.
- Added functions to atomically read the position data.
- Added sensitivity settings.
- Added IIC clock setting. Testing with SAMD confirms it works up to at least 1MHz.

## Overview
The DFRobot IR positioning camera has a resolution of 1024x768 and tracks up to 4 infrared objects. According to the WiiBrew wiki it works best with 940nm infrared emitters.

---

# Non-atomic position data discovery
During my testing with the IR positioning camera, I was initially puzzled by two observations.

The first thing I noticed is sometimes a Y position would be returned with a value greater than 767 but less than 1023. The vertical resolution is 768 so values in that range are unexpected. A Y value of 1023 is expected when no object is seen. Otherwise, the Y value is expected to be less than 768. It was uncommon but not difficult to reproduce Y values of 768 and sometimes values a bit over 800 were returned. What's going on here?

Secondly, I wrote some test code to determine the rate at which the position data updates. The code was a simple loop to request the data and compare with the previously received data. If there are any differences in the data then assume the position updated. Obviously I was shaking the camera around to ensure movement would be detected with each update. When I began testing this, I noticed that lower IIC clock rates reported higher position update rates. For example, using an IIC clock rate of 1MHz my test code reported ~215 FPS. When I slowed down the IIC clock to 100kbps (Arduino default), the test reported over 300 FPS for the position changing. This was initially counterintuitive to me.

Then it finally clicked. The position data is not being reported atomically. In other words, the position data in the IR camera may update between reading the low byte and high byte values for a single position. This totally explains everything I was observing. 768 is a value with the low 8 bits clear and the high bits 8:9, so this can happen when observing an object that goes from Y position 0 to off camera. It also explains why a slower IIC clock rate detects more changes; really all that's happening is you are more likely to catch the non-atomic position data update while reading the bytes from IIC.

My theory was confirmed by adding "atomic" position read functions to the library to fetch the data multiple times until two consecutive sets of data are identical. I call the functions "atomic" although it's really just a workaround to ensure coherent position data is used.

The final result is that using the "atomic" functions I found the camera updates the position at ~209 FPS.

# Final Thoughts
I can only speculate that there must be a proper way to read the position data atomically without fetching it multiple times and comparing the data for a match. Maybe there's a data sync pin that is not exposed? Maybe there's a register setting not documented on the WiiBrew wiki for this? It's also odd that there is no formal data sheet that I'm aware of. The best and most complete information I have seen is in the WiiBrew wiki.
