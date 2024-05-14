/*!
 * @file OpenFIRE_Square.cpp.cpp
 * @brief Light Gun library for 4 LED setup
 * @n CPP file for Samco Light Gun 4 LED setup
 *
 * @copyright Samco, https://github.com/samuelballantyne, 2024
 * @copyright GNU Lesser General Public License
 *
 * @author [Sam Ballantyne](samuelballantyne@hotmail.com)
 * @version V1.0
 * @date 2021
 */

#include <Arduino.h>
#include "OpenFIRE_Square.h"

constexpr int buff = 50 * CamToMouseMult;

// floating point PI
constexpr float fPI = (float)PI;

void OpenFIRE_Square::begin(const int* px, const int* py, unsigned int seen)
{
    // Remapping LED postions to use with library.
  
    positionXX[0] = px[0] << CamToMouseShift;
    positionYY[0] = py[0] << CamToMouseShift;
    positionXX[1] = px[1] << CamToMouseShift;
    positionYY[1] = py[1] << CamToMouseShift;
    positionXX[2] = px[2] << CamToMouseShift;
    positionYY[2] = py[2] << CamToMouseShift;
    positionXX[3] = px[3] << CamToMouseShift;
    positionYY[3] = py[3] << CamToMouseShift;

    seenFlags = seen;

    // Wait for all postions to be recognised before starting

    if(seenFlags == 0x0F) {
        start = 0xFF;
    } else if(!start) {
        // all positions not yet seen
        return;
    }

    for(unsigned int i = 0; i < 4; i++) {
        // if LED not seen...
        if (!(seenFlags & (1 << i))) {
            // if unseen make sure all quadrants have a value if missing apply value with buffer and set to unseen (this step is important for 1 LED usage)
            if (!(((positionY[0] < medianY) && (positionX[0] < medianX)) || ((positionY[1] < medianY) && (positionX[1] < medianX)) || ((positionY[2] < medianY) && (positionX[2] < medianX)) || ((positionY[3] < medianY) && (positionX[3] < medianX)))) {
                positionX[i] = medianX + (medianX - FinalX[3]) - buff;
                positionY[i] = medianY + (medianY - FinalY[3]) - buff;
                see[0] = 0;
            }
            if (!(((positionY[0] < medianY) && (positionX[0] > medianX)) || ((positionY[1] < medianY) && (positionX[1] > medianX)) || ((positionY[2] < medianY) && (positionX[2] > medianX)) || ((positionY[3] < medianY) && (positionX[3] > medianX)))) {
                positionX[i] = medianX + (medianX - FinalX[2]) + buff;
                positionY[i] = medianY + (medianY - FinalY[2]) - buff;
                see[1] = 0;
            }
            if (!(((positionY[0] > medianY) && (positionX[0] < medianX)) || ((positionY[1] > medianY) && (positionX[1] < medianX)) || ((positionY[2] > medianY) && (positionX[2] < medianX)) || ((positionY[3] > medianY) && (positionX[3] < medianX)))) {
                positionX[i] = medianX + (medianX - FinalX[1]) - buff;
                positionY[i] = medianY + (medianY - FinalY[1]) + buff;
                see[2] = 0;
            }
            if (!(((positionY[0] > medianY) && (positionX[0] > medianX)) || ((positionY[1] > medianY) && (positionX[1] > medianX)) || ((positionY[2] > medianY) && (positionX[2] > medianX)) || ((positionY[3] > medianY) && (positionX[3] > medianX)))) {
                positionX[i] = medianX + (medianX - FinalX[0]) + buff;
                positionY[i] = medianY + (medianY - FinalY[0]) + buff;
                see[3] = 0;
            }

            // if all quadrants have a value apply value with buffer and set to see/unseen            
            if (positionY[i] < medianY) {
                if (positionX[i] < medianX) {
                    positionX[i] = medianX + (medianX - FinalX[3]) - buff;
                    positionY[i] = medianY + (medianY - FinalY[3]) - buff;
                    see[0] = 0;
                }
                if (positionX[i] > medianX) {
                    positionX[i] = medianX + (medianX - FinalX[2]) + buff;
                    positionY[i] = medianY + (medianY - FinalY[2]) - buff;
                    see[1] = 0;
                }
            }
            if (positionY[i] > medianY) {
                if (positionX[i] < medianX) {
                    positionX[i] = medianX + (medianX - FinalX[1]) - buff;
                    positionY[i] = medianY + (medianY - FinalY[1]) + buff;
                    see[2] = 0;
                }
                if (positionX[i] > medianX) {
                    positionX[i] = medianX + (medianX - FinalX[0]) + buff;
                    positionY[i] = medianY + (medianY - FinalY[0]) + buff;
                    see[3] = 0;
                }
            }

        } else {
            // If LEDS have been seen place in correct quadrant, apply buffer an set to seen.
            int mapXX = map(positionXX[i], 0, MouseMaxX, MouseMaxX, 0);
            if (positionYY[i] < medianY) {
                if (mapXX < medianX) {
                    positionX[i] = mapXX - buff;
                    positionY[i] = positionYY[i] - buff;
                    see[0] <<= 1;
                    see[0] |= 1;
                } else if (mapXX > medianX) {
                    positionX[i] = mapXX + buff;
                    positionY[i] = positionYY[i] - buff;
                    see[1] <<= 1;
                    see[1] |= 1;
                }
            } else if (positionYY[i] > medianY) {
                if (mapXX < medianX) {
                    positionX[i] = mapXX - buff;
                    positionY[i] = positionYY[i] + buff;
                    see[2] <<= 1;
                    see[2] |= 1;
                } else if (mapXX > medianX) {
                    positionX[i] = mapXX + buff;
                    positionY[i] = positionYY[i] + buff;
                    see[3] <<= 1;
                    see[3] |= 1;
                }
            }
        }

        // Arrange all values in to quadrants and remove buffer.
        // If LEDS have been seen use there value
        // If LEDS haven't been seen work out values form live positions
        
        if (positionY[i] < medianY) {
            if (positionX[i] < medianX) {
                if (see[0] & 0x02) { 
                    FinalX[0] = positionX[i] + buff;
                    FinalY[0] = positionY[i] + buff;
                } else if (positionY[i] < 0) {
                    float f = angleBottom + angleOffset[2];
                    FinalX[0] = FinalX[2] + round(yDistLeft * cos(f));
                    FinalY[0] = FinalY[2] + round(yDistLeft * -sin(f));
                } else if (positionX[i] < 0) {
                    float f = angleRight - angleOffset[1];
                    FinalX[0] = FinalX[1] + round(xDistTop * -cos(f));
                    FinalY[0] = FinalY[1] + round(xDistTop * sin(f));
                }
            } else if (positionX[i] > medianX) {
                if (see[1] & 0x02) {
                    FinalX[1] = positionX[i] - buff;
                    FinalY[1] = positionY[i] + buff;
                } else if (positionY[i] < 0) {
                    float f = angleBottom - (angleOffset[3] - fPI);
                    FinalX[1] = FinalX[3] + round(yDistRight * cos(f));
                    FinalY[1] = FinalY[3] + round(yDistRight * -sin(f));
                } else if (positionX[i] > MouseMaxX) {
                    float f = angleLeft + (angleOffset[0] - fPI);
                    FinalX[1] = FinalX[0] + round(xDistTop * cos(f));
                    FinalY[1] = FinalY[0] + round(xDistTop * -sin(f));
                }
            }
        } else if (positionY[i] > medianY) {
            if (positionX[i] < medianX) {
                if (see[2] & 0x02) {
                    FinalX[2] = positionX[i] + buff;
                    FinalY[2] = positionY[i] - buff;
                } else if (positionY[i] > MouseMaxY) {
                    float f = angleTop - angleOffset[0];
                    FinalX[2] = FinalX[0] + round(yDistLeft * cos(f));
                    FinalY[2] = FinalY[0] + round(yDistLeft * -sin(f));
                } else if (positionX[i] < 0) {
                    float f = angleRight + angleOffset[3];
                    FinalX[2] = FinalX[3] + round(xDistBottom * cos(f));
                    FinalY[2] = FinalY[3] + round(xDistBottom * -sin(f));
                }
            } else if (positionX[i] > medianX) {
                if ((see[3] & 0x02)) {
                    FinalX[3] = positionX[i] - buff;
                    FinalY[3] = positionY[i] - buff;
                } else if (positionY[i] > MouseMaxY) {
                    float f = angleTop + (angleOffset[1] - fPI);
                    FinalX[3] = FinalX[1] + round(yDistRight * cos(f));
                    FinalY[3] = FinalY[1] + round(yDistRight * -sin(f));
                } else if (positionX[i] > MouseMaxX) {
                    float f = angleLeft - (angleOffset[2] - fPI);
                    FinalX[3] = FinalX[2] + round(xDistBottom * -cos(f));
                    FinalY[3] = FinalY[2] + round(xDistBottom * sin(f));
                }
            }
        }
    }

    // If all LEDS can be seen update median & angle offsets (resets sketch stop hangs on glitches)
    
    if (seenFlags == 0x0F) {
        medianY = (positionY[0] + positionY[1] + positionY[2] + positionY[3] + 2) / 4;
        medianX = (positionX[0] + positionX[1] + positionX[2] + positionX[3] + 2) / 4;
    } else {
        medianY = (FinalY[0] + FinalY[1] + FinalY[2] + FinalY[3] + 2) / 4;
        medianX = (FinalX[0] + FinalX[1] + FinalX[2] + FinalX[3] + 2) / 4;
    }

    // If 4 LEDS can be seen and loop has run through 5 times update offsets and height      

    if ((1 << 5) & see[0] & see[1] & see[2] & see[3]) {
        angleOffset[0] = angleTop - (angleLeft - fPI);
        angleOffset[1] = -(angleTop - angleRight);
        angleOffset[2] = -(angleBottom - angleLeft);
        angleOffset[3] = angleBottom - (angleRight - fPI);
        height = (yDistLeft + yDistRight) / 2.0f;
        width = (xDistTop + xDistBottom) / 2.0f;
    }

    // If 2 LEDS can be seen and loop has run through 5 times update angle and distances

    if ((1 << 5) & see[0] & see[2]) {
        angleLeft = atan2(FinalY[2] - FinalY[0], FinalX[0] - FinalX[2]);
        yDistLeft = hypot((FinalY[0] - FinalY[2]), (FinalX[0] - FinalX[2]));
    }

    if ((1 << 5) & see[3] & see[1]) {
        angleRight = atan2(FinalY[3] - FinalY[1], FinalX[1] - FinalX[3]);
        yDistRight = hypot((FinalY[3] - FinalY[1]), (FinalX[3] - FinalX[1]));
    }
    
    if ((1 << 5) & see[0] & see[1]) {
        angleTop = atan2(FinalY[0] - FinalY[1], FinalX[1] - FinalX[0]);
        xDistTop = hypot((FinalY[0] - FinalY[1]), (FinalX[0] - FinalX[1]));
    }

    if ((1 << 5) & see[3] & see[2]) {
        angleBottom = atan2(FinalY[2] - FinalY[3], FinalX[3] - FinalX[2]);
        xDistBottom = hypot((FinalY[2] - FinalY[3]), (FinalX[2] - FinalX[3]));
    }

    // Add tilt correction
    angle = (atan2(FinalY[0] - FinalY[1], FinalX[1] - FinalX[0]) + atan2(FinalY[2] - FinalY[3], FinalX[3] - FinalX[2])) / 2.0f;
    float cosAngle = cos(angle);
    float sinAngle = sin(angle);
}
