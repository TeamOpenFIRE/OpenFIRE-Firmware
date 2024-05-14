/*!
 * @file OpenFIRE_Square.h
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

#ifndef _OpenFIRE_Square_h_
#define _OpenFIRE_Square_h_

#include <stdint.h>
#include "OpenFIREConst.h"

class OpenFIRE_Square {
  
    int positionXX[4];   ///< position x.
    int positionYY[4];   ///< position y.

    int positionX[4];
    int positionY[4];

    unsigned int see[4];

    int medianY = MouseMaxY / 2;
    int medianX = MouseMaxX / 2;

    int FinalX[4] = {400 * CamToMouseMult, 623 * CamToMouseMult, 400 * CamToMouseMult, 623 * CamToMouseMult};
    int FinalY[4] = {200 * CamToMouseMult, 200 * CamToMouseMult, 568 * CamToMouseMult, 568 * CamToMouseMult};

    float xDistTop;
    float xDistBottom;
    float yDistLeft;
    float yDistRight;

    float angleTop;
    float angleBottom;
    float angleLeft;
    float angleRight;

    float angle;
    float height;
    float width;

    float angleOffset[4];

    unsigned int start = 0;

    unsigned int seenFlags = 0;

public:

    /// @brief Main function to calculate X, Y, and H
    void begin(const int* px, const int* py, unsigned int seen);
    
    int X(int index) const { return FinalX[index]; }
    int Y(int index) const { return FinalY[index]; }
    unsigned int testSee(int index) const { return see[index]; }
    int testMedianX() const { return medianX; }
    int testMedianY() const { return medianY; }
    
    /// @brief Height
    float H() const { return height; }

    /// @brief Height
    float W() const { return width; }

    /// @brief Angle
    float Ang() const { return angle; }
    
    /// @brief Bit mask of positions the camera saw
    unsigned int seen() const { return seenFlags; }
};

#endif // _OpenFIRE_Square_h_
