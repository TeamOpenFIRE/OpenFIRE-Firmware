# absmouse
Absolute positioning USB HID mouse library for Arduino devices with USB capabilities.

## Example use
```c++
#include <AbsMouse.h>

void setup() {
  AbsMouse.init(1920, 1080);
}

void loop() {
  // do some stuff
  AbsMouse.move(500, 200);
  AbsMouse.press(MOUSE_LEFT);
  AbsMouse.release(MOUSE_LEFT);
  // do some other stuff
}
```
