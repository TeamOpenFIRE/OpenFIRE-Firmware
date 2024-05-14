/*!
 * @file OpenFIRE_Diamond.cpp
 * @brief Light Gun library for 4 LED setup
 * @n CPP file for Samco Light Gun 4 LED setup
 *
 * @copyright Samco, https://github.com/samuelballantyne, 2024
 * @copyright GNU Lesser General Public License
 *
 * @author [Sam Ballantyne](samuelballantyne@hotmail.com)
 * @version V1.0
 * @date 2024
 */

#include <Arduino.h>
#include "OpenFIRE_Diamond.h"

constexpr int buff = 50 * CamToMouseMult;

// floating point PI
constexpr float fPI = (float)PI;

void OpenFIRE_Diamond::begin(const int* px, const int* py, unsigned int seen)
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
        if (!((positionY[0] < medianY - (height / 2) + buff) || (positionY[1] < medianY - (height / 2) + buff) || (positionY[2] < medianY - (height / 2) + buff) || (positionY[3] < medianY - (height / 2) + buff))) {
          positionX[i] = medianX + (medianX - FinalX[2]);
          positionY[i] = FinalY[2] - height - buff;
          see[0] = 0;
        }

        if (!((positionX[0] > medianX + (width / 2) - buff) || (positionX[1] > medianX + (width / 2) - buff) || (positionX[2] > medianX + (width / 2) - buff) || (positionX[3] > medianX + (width / 2) - buff))) {
          positionX[i] = FinalX[3] + width + buff;
          positionY[i] = medianY + (medianY - FinalY[3]);
          see[1] = 0;
        }

        if (!((positionY[0] > medianY + (height / 2) - buff) || (positionY[1] > medianY + (height / 2) - buff) || (positionY[2] > medianY + (height / 2) - buff) || (positionY[3] > medianY + (height / 2) - buff))) {
          positionX[i] = medianX + (medianX - FinalX[0]);
          positionY[i] = FinalY[0] + height + buff;
          see[2] = 0;
        }

        if (!((positionX[0] < medianX - (width / 2) + buff) || (positionX[1] < medianX - (width / 2) + buff) || (positionX[2] < medianX - (width / 2) + buff) || (positionX[3] < medianX - (width / 2) + buff))) {
          positionX[i] = FinalX[1] - width - buff;
          positionY[i] = medianY + (medianY - FinalY[1]);
          see[3] = 0;
        }

        // if all quadrants have a value apply value with buffer and set to see/unseen

        if (positionY[i] < (medianY - (height / 2) + buff)) {
          positionX[i] = medianX + (medianX - FinalX[2]);
          positionY[i] = FinalY[2] - height - buff;
          see[0] = 0;
        }

        if (positionX[i] > (medianX + (width / 2) - buff)) {
          positionX[i] = FinalX[3] + width + buff;
          positionY[i] = medianY + (medianY - FinalY[3]);
          see[1] = 0;
        }

        if (positionY[i] > (medianY + (height / 2) - buff)) {
          positionX[i] = medianX + (medianX - FinalX[0]);
          positionY[i] = FinalY[0] + height + buff;
          see[2] = 0;
        }

        if (positionX[i] < (medianX - (width / 2) + buff)) {
          positionX[i] = FinalX[1] - width - buff;
          positionY[i] = medianY + (medianY - FinalY[1]);
          see[3] = 0;
        }


        } else {
            // If LEDS have been seen place in correct quadrant, apply buffer an set to seen.
        if (positionYY[i] < (medianY - (height / 2) + buff)) {
          positionX[i] = positionXX[i];
          positionY[i] = positionYY[i] - buff;
          see[0] <<= 1;
          see[0] |= 1;
          yMin = i;
        }
        if (positionXX[i] > (medianX + (width / 2) - buff)) {
          positionX[i] = positionXX[i] + buff;
          positionY[i] = positionYY[i];
          see[1] <<= 1;
          see[1] |= 1;
          xMax = i;
        }
        if (positionYY[i] > (medianY + (height / 2) - buff)) {
          positionX[i] = positionXX[i];
          positionY[i] = positionYY[i] + buff;
          see[2] <<= 1;
          see[2] |= 1;
          yMax = i;
        }
        if (positionXX[i] < (medianX - (width / 2) + buff)) {
          positionX[i] = positionXX[i] - buff;
          positionY[i] = positionYY[i];
          see[3] <<= 1;
          see[3] |= 1;
          xMin = i;
        }
      }

        // Arrange all values in to quadrants and remove buffer.
        // If LEDS have been seen use there value
        // If LEDS haven't been seen work out values form live positions
        
            if (positionY[i] < (medianY - (height / 2))) {
      if (see[0] & 0x02) {
        FinalX[0] = positionX[i];
        FinalY[0] = positionY[i] + buff;
      } else if (see[3] & 0x02) {
        FinalX[0] = FinalX[3] + (DistTL * -1) * cos(offsetTL + angle);
        FinalY[0] = FinalY[3] + (DistTL * -1) * sin(offsetTL + angle);
      } else {
        FinalX[0] = FinalX[1] + DistTR * cos(offsetTR + angle);
        FinalY[0] = FinalY[1] + DistTR * sin(offsetTR + angle);
      }
    }

    if (positionX[i] > (medianX + (width / 2))) {
      if (see[1] & 0x02) {
        FinalX[1] = positionX[i] - buff;
        FinalY[1] = positionY[i];
      } else if (see[0] & 0x02) {
        FinalX[1] = FinalX[0] + (DistTR * -1) * cos(offsetTR + angle);
        FinalY[1] = FinalY[0] + (DistTR * -1) * sin(offsetTR + angle);
      } else {
        FinalX[1] = FinalX[2] + (DistBR) * cos(offsetBR + angle);
        FinalY[1] = FinalY[2] + (DistBR) * sin(offsetBR + angle);
      }
    }

    if (positionY[i] > (medianY + (height / 2))) {
      if (see[2] & 0x02) {
        FinalX[2] = positionX[i];
        FinalY[2] = positionY[i] - buff;
      } else if (see[1] & 0x02) {
        FinalX[2] = FinalX[1] + (DistBR * -1) * cos(offsetBR + angle);
        FinalY[2] = FinalY[1] + (DistBR * -1) * sin(offsetBR + angle);
      } else {
        FinalX[2] = FinalX[3] + (DistBL) * cos(offsetBL + angle);
        FinalY[2] = FinalY[3] + (DistBL) * sin(offsetBL + angle);
      }
    }

    if (positionX[i] < (medianX - (width / 2))) {
      if (see[3] & 0x02) {
        FinalX[3] = positionX[i] + buff;
        FinalY[3] = positionY[i];
      } else if (see[1] & 0x02) {
        FinalX[3] = FinalX[2] + (DistBL * -1) * cos(offsetBL + angle);
        FinalY[3] = FinalY[2] + (DistBL * -1) * sin(offsetBL + angle);
      } else {
        FinalX[3] = FinalX[0] + (DistTL) * cos(offsetTL + angle);
        FinalY[3] = FinalY[0] + (DistTL) * sin(offsetTL + angle);
      }
    }

    }

    // If all LEDS can be seen update median & angle offsets (resets sketch stop hangs on glitches)
    
    if (seenFlags == 0x0F) {
      medianY = (positionYY[0] + positionYY[1] + positionYY[2] + positionYY[3]) / 4;
      medianX = (positionXX[0] + positionXX[1] + positionXX[2] + positionXX[3]) / 4;
      angle2 = atan2(positionYY[xMin] - positionYY[xMax], positionXX[xMax] - positionXX[xMin]);
    } else {
      medianY = (FinalY[0] + FinalY[1] + FinalY[2] + FinalY[3]) / 4;
      medianX = (FinalX[0] + FinalX[1] + FinalX[2] + FinalX[3]) / 4;
      angle2 = atan2(FinalY[3] - FinalY[1], FinalX[1] - FinalX[3]);
    }

    // If 4 LEDS can be seen and loop has run through 5 times update offsets and height      

    if ((1 << 5) & see[0] & see[1] & see[2] & see[3]) {
      height = hypot(positionYY[yMin] - positionYY[yMax], positionXX[yMin] - positionXX[yMax]);
      width = hypot(positionYY[xMin] - positionYY[xMax], positionXX[xMin] - positionXX[xMax]);
      angle2 = atan2(positionYY[xMin] - positionYY[xMax], positionXX[xMax] - positionXX[xMin]);
      offsetTR = angleTR - atan2(positionYY[xMax] - positionYY[xMin], positionXX[xMax] - positionXX[xMin]);
      offsetBR = angleBR - atan2(positionYY[xMax] - positionYY[xMin], positionXX[xMax] - positionXX[xMin]);
      offsetBL = angleBL - atan2(positionYY[xMax] - positionYY[xMin], positionXX[xMax] - positionXX[xMin]);
      offsetTL = angleTL - atan2(positionYY[xMax] - positionYY[xMin], positionXX[xMax] - positionXX[xMin]);
    }

    // If 2 LEDS can be seen and loop has run through 5 times update angle and distances

    if ((1 << 5) & see[0] & see[1]) {
      angleTR = atan2(FinalY[0] - FinalY[1], FinalX[0] - FinalX[1]);
      DistTR = hypot((FinalY[0] - FinalY[1]), (FinalX[0] - FinalX[1]));
      angle = angleTR - offsetTR;
    }

    if ((1 << 5) & see[1] & see[2]) {
      angleBR = atan2(FinalY[1] - FinalY[2], FinalX[1] - FinalX[2]);
      DistBR = hypot((FinalY[1] - FinalY[2]), (FinalX[1] - FinalX[2]));
      angle = angleBR - offsetBR;
    }

    if ((1 << 5) & see[3] & see[2]) {
      angleBL = atan2(FinalY[2] - FinalY[3], FinalX[2] - FinalX[3]);
      DistBL = hypot((FinalY[2] - FinalY[3]), (FinalX[2] - FinalX[3]));
      angle = angleBL - offsetBL;
    }

    if ((1 << 5) & see[3] & see[0]) {
      angleTL = atan2(FinalY[3] - FinalY[0], FinalX[3] - FinalX[0]);
      DistTL = hypot((FinalY[3] - FinalY[0]), (FinalX[3] - FinalX[0]));
      angle = angleTL - offsetTL;
    }


    // Add tilt correction
    //angle = (atan2(FinalY[0] - FinalY[1], FinalX[1] - FinalX[0]) + atan2(FinalY[2] - FinalY[3], FinalX[3] - FinalX[2])) / 2.0f;
    //float cosAngle = cos(angle);
    //float sinAngle = sin(angle);

    FinalXX[1] = ((FinalX[3] + FinalX[0]) / 2) + (((FinalX[3] + FinalX[0]) / 2) - medianX);
    FinalYY[1] = ((FinalY[3] + FinalY[0]) / 2) + (((FinalY[3] + FinalY[0]) / 2) - medianY);
    FinalXX[0] = ((FinalX[0] + FinalX[1]) / 2) + (((FinalX[0] + FinalX[1]) / 2) - medianX);
    FinalYY[0] = ((FinalY[0] + FinalY[1]) / 2) + (((FinalY[0] + FinalY[1]) / 2) - medianY);
    FinalXX[3] = ((FinalX[2] + FinalX[3]) / 2) + (((FinalX[2] + FinalX[3]) / 2) - medianX);
    FinalYY[3] = ((FinalY[2] + FinalY[3]) / 2) + (((FinalY[2] + FinalY[3]) / 2) - medianY);
    FinalXX[2] = ((FinalX[1] + FinalX[2]) / 2) + (((FinalX[1] + FinalX[2]) / 2) - medianX);
    FinalYY[2] = ((FinalY[1] + FinalY[2]) / 2) + (((FinalY[1] + FinalY[2]) / 2) - medianY);
}
