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
constexpr float fHPI = (float)HALF_PI;
//constexpr float fPI = (float)PI;

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
        if (!(((positionY[0] < medianY - (height2 / 2) + buff) && (tilt ? positionX[0] >= medianX - (width2 / 2) + buff : positionX[0] < medianX + (width2 / 2) - buff)) || 
		      ((positionY[1] < medianY - (height2 / 2) + buff) && (tilt ? positionX[1] >= medianX - (width2 / 2) + buff : positionX[1] < medianX + (width2 / 2) - buff)) || 
		      ((positionY[2] < medianY - (height2 / 2) + buff) && (tilt ? positionX[2] >= medianX - (width2 / 2) + buff : positionX[2] < medianX + (width2 / 2) - buff)) || 
		      ((positionY[3] < medianY - (height2 / 2) + buff) && (tilt ? positionX[3] >= medianX - (width2 / 2) + buff : positionX[3] < medianX + (width2 / 2) - buff)))) {
          float f = angle + fHPI;
		  positionX[i] = medianX - round((height / 2) * cos(f));
          positionY[i] = medianY - round((height / 2) * sin(f)) - buff;
          see[0] = 0;
          yMin = i;
        }

        if (!((positionX[0] > medianX + (width2 / 2) - buff) || 
	          (positionX[1] > medianX + (width2 / 2) - buff) || 
              (positionX[2] > medianX + (width2 / 2) - buff) || 
              (positionX[3] > medianX + (width2 / 2) - buff))) {
		  float f = angle;
          positionX[i] = medianX + round((width / 2) * cos(f)) + buff;
          positionY[i] = medianY + round((width / 2) * sin(f));
          see[1] = 0;
          xMax = i;
        }

        if (!(((positionY[0] > medianY + (height2 / 2) - buff) && (tilt ? positionX[0] <= medianX + (width2 / 2) - buff : positionX[0] > medianX - (width2 / 2) + buff)) || 
		      ((positionY[1] > medianY + (height2 / 2) - buff) && (tilt ? positionX[1] <= medianX + (width2 / 2) - buff : positionX[1] > medianX - (width2 / 2) + buff)) || 
		      ((positionY[2] > medianY + (height2 / 2) - buff) && (tilt ? positionX[2] <= medianX + (width2 / 2) - buff : positionX[2] > medianX - (width2 / 2) + buff)) || 
		      ((positionY[3] > medianY + (height2 / 2) - buff) && (tilt ? positionX[3] <= medianX + (width2 / 2) - buff : positionX[3] > medianX - (width2 / 2) + buff)))) {
          float f = angle  + fHPI;
		  positionX[i] = medianX - round((height / 2) * -cos(f));
          positionY[i] = medianY - round((height / 2) * -sin(f)) + buff;
          see[2] = 0;
          yMax = i;
        }

        if (!((positionX[0] < medianX - (width2 / 2) + buff) || 
		      (positionX[1] < medianX - (width2 / 2) + buff) || 
		      (positionX[2] < medianX - (width2 / 2) + buff) || 
		      (positionX[3] < medianX - (width2 / 2) + buff))) {
		  float f = angle;
          positionX[i] = medianX + round((width / 2) * -cos(f)) - buff;
          positionY[i] = medianY + round((width / 2) * -sin(f));
          see[3] = 0;
          xMin = i;
        } 
        // if all quadrants have a value apply value with buffer and set to see/unseen

		if (positionY[i] < (medianY - (height2 / 2) + buff) && (tilt ? positionX[i] >= medianX - (width2 / 2) + buff : positionX[i] < medianX + (width2 / 2) - buff)) {
            float f = angle + fHPI;
			positionX[i] = medianX - round((height / 2) * cos(f));
            positionY[i] = medianY - round((height / 2) * sin(f)) - buff;
          see[0] = 0;
          yMin = i;
        }

        if (positionX[i] > (medianX + (width2 / 2) - buff)) {
			float f = angle;
            positionX[i] = medianX + round((width / 2) * cos(f)) + buff;
            positionY[i] = medianY + round((width / 2) * sin(f));
          see[1] = 0;
          xMax = i;
        }

        if (positionY[i] > (medianY + (height2 / 2) - buff) && (tilt ? positionX[i] <= medianX + (width2 / 2) - buff : positionX[i] > medianX - (width2 / 2) + buff)) {
            float f = angle + fHPI;
			positionX[i] = medianX - round((height / 2) * -cos(f));
            positionY[i] = medianY - round((height / 2) * -sin(f)) + buff;
            see[2] = 0;
            yMax = i;
        }

        if (positionX[i] < (medianX - (width2 / 2) + buff)) {
			float f = angle;
            positionX[i] = medianX + round((width / 2) * -cos(f)) - buff;
            positionY[i] = medianY + round((width / 2) * -sin(f));
          see[3] = 0;
          xMin = i;
        }  


        } else {

          // If LEDS have been seen place in correct quadrant, apply buffer an set to seen.
			
          if (positionYY[i] < (medianY - (height2 / 2) + buff) && (tilt ? positionXX[i] >= medianX - buff : positionXX[i] < medianX + buff)) {
            positionX[i] = positionXX[i];
            positionY[i] = positionYY[i] - buff;
            see[0] <<= 1;
            see[0] |= 1;
            yMin = i;
          }
          if (positionXX[i] > (medianX + (width2 / 2) - buff)) {
            positionX[i] = positionXX[i] + buff;
            positionY[i] = positionYY[i];
            see[1] <<= 1;
            see[1] |= 1;
            xMax = i;
          }
          if (positionYY[i] > (medianY + (height2 / 2) - buff) && (tilt ? positionXX[i] <= medianX + buff : positionXX[i] > medianX - buff)) {
            positionX[i] = positionXX[i];
            positionY[i] = positionYY[i] + buff;
            see[2] <<= 1;
            see[2] |= 1;
            yMax = i;
          }
          if (positionXX[i] < (medianX - (width2 / 2) + buff)) {
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

    if (positionY[i] < (medianY - (height2 / 2)) && (tilt ? positionX[i] >= medianX - buff : positionX[i] < medianX + buff)) {
      if (see[0] & 0x02) {
        FinalX[0] = positionX[yMin];
        FinalY[0] = positionY[yMin] + buff;
      } else if (see[3] & 0x02) {
		float f = angle;
		float o = offsetTL;
        FinalX[0] = FinalX[3] + round(DistTL * cos(o - f));
        FinalY[0] = FinalY[3] + round(DistTL * -sin(o - f));
      } else {
  		float f = angle;
  		float o = offsetTR;
        FinalX[0] = FinalX[1] + round(DistTR * -cos(o - f));
        FinalY[0] = FinalY[1] + round(DistTR * sin(o - f));
      }
    }

    if (positionX[i] > (medianX + (width2 / 2))) {
      if (see[1] & 0x02) {
        FinalX[1] = positionX[xMax] - buff;
        FinalY[1] = positionY[xMax];
      } else if (see[0] & 0x02) {
  		float f = angle;
  		float o = offsetTR;
        FinalX[1] = FinalX[0] + round(DistTR * cos(o - f));
        FinalY[1] = FinalY[0] + round(DistTR * -sin(o - f));
      } else {
  		float f = angle;
  		float o = offsetBR;
        FinalX[1] = FinalX[2] + round(DistBR * -cos(o - f));
        FinalY[1] = FinalY[2] + round(DistBR * sin(o - f));
      }
    }

    if (positionY[i] > (medianY + (height2 / 2)) && (tilt ? positionX[i] <= medianX + buff : positionX[i] > medianX - buff)) {
      if (see[2] & 0x02) {
        FinalX[2] = positionX[yMax];
        FinalY[2] = positionY[yMax] - buff;
      } else if (see[1] & 0x02) {
  		float f = angle;
  		float o = offsetBR;
        FinalX[2] = FinalX[1] + round(DistBR * cos(o - f));
        FinalY[2] = FinalY[1] + round(DistBR * -sin(o - f));
      } else {
  		float f = angle;
  		float o = offsetBL;
        FinalX[2] = FinalX[3] + round(DistBL * -cos(o - f));
        FinalY[2] = FinalY[3] + round(DistBL * sin(o - f));
      }
    }

    if (positionX[i] < (medianX - (width2 / 2))) {
      if (see[3] & 0x02) {
        FinalX[3] = positionX[xMin] + buff;
        FinalY[3] = positionY[xMin];
      } else if (see[2] & 0x02) {
  		float f = angle;
  		float o = offsetBL;
        FinalX[3] = FinalX[2] + round(DistBL * cos(o - f));
        FinalY[3] = FinalY[2] + round(DistBL * -sin(o - f));
      } else {
  		float f = angle;
  		float o = offsetTL;
        FinalX[3] = FinalX[0] + round(DistTL * -cos(o - f));
        FinalY[3] = FinalY[0] + round(DistTL * sin(o - f));
      }
   
    }

    }
	
    if (angle <= 0) {
      tilt = false;
    } else { 
      tilt = true;
    }

    // If all LEDS can be seen update median & angle offsets (resets sketch stop hangs on glitches)
    
    if (seenFlags == 0x0F) {
      medianY = (positionYY[0] + positionYY[1] + positionYY[2] + positionYY[3]) / 4;
      medianX = (positionXX[0] + positionXX[1] + positionXX[2] + positionXX[3]) / 4;
      angle = 0;
      height = 0;
      height2 = 0;
      width = 0;
      width2 = 0;
      angle2 = 0;
    } else {
      medianY = (FinalY[0] + FinalY[1] + FinalY[2] + FinalY[3]) / 4;
      medianX = (FinalX[0] + FinalX[1] + FinalX[2] + FinalX[3]) / 4;
    }

    // If 4 LEDS can be seen and loop has run through 5 times update offsets and height      

    if (seenFlags == 0x0F) {
      height = hypot(positionYY[yMin] - positionYY[yMax], positionXX[yMin] - positionXX[yMax]);
      height2 = positionYY[yMax] - positionYY[yMin];
      width = hypot(positionYY[xMin] - positionYY[xMax], positionXX[xMin] - positionXX[xMax]);
      width2 = positionXX[xMax] - positionXX[xMin];
      angle2 = atan2(positionYY[xMin] - positionYY[xMax], positionXX[xMax] - positionXX[xMin]);
      offsetTR = angleTR - angle2;
      offsetBR = angleBR - angle2;
      offsetBL = angleBL - angle2;
      offsetTL = angleTL - angle2;
    }

    // If 2 LEDS can be seen update angle and distances

    if ((1 << 5) & see[0] & see[1]) {
      angleTR = atan2(positionYY[yMin] - positionYY[xMax], positionXX[xMax] - positionXX[yMin]);
      DistTR = hypot((positionYY[yMin] - positionYY[xMax]), (positionXX[yMin] - positionXX[xMax]));
      angle = (offsetTR - angleTR );
    }

    if ((1 << 5) & see[1] & see[2]) {
      angleBR = atan2(positionYY[xMax] - positionYY[yMax], positionXX[yMax] - positionXX[xMax]);
      DistBR = hypot((positionYY[xMax] - positionYY[yMax]), (positionXX[xMax] - positionXX[yMax]));
      angle = (offsetBR - angleBR);
    }

    if ((1 << 5) & see[3] & see[2]) {
      angleBL = atan2(positionYY[yMax] - positionYY[xMin], positionXX[xMin] - positionXX[yMax]);
      DistBL = hypot((positionYY[yMax] - positionYY[xMin]), (positionXX[yMax] - positionXX[xMin]));
      angle = (offsetBL - angleBL);
    }

    if ((1 << 5) & see[3] & see[0]) {
      angleTL = atan2(positionYY[xMin] - positionYY[yMin], positionXX[yMin] - positionXX[xMin]);
      DistTL = hypot((positionYY[xMin] - positionYY[yMin]), (positionXX[xMin] - positionXX[yMin]));
	  angle = (offsetTL - angleTL);
    }


}
