/*!
 * @file SamcoPositionEnhanced.h
 * @brief Samco Light Gun library for 4 LED setup
 * @n Header file for Samco Light Gun 4 LED setup
 *
 * @copyright [Samco](http://www.samco.co.nz), 2021
 * @copyright GNU Lesser General Public License
 *
 * @author [Sam Ballantyne](samuelballantyne@hotmail.com)
 * @version V1.0
 * @date 2020
 */

#ifndef _SamcoPositionEnhanced_h_
#define _SamcoPositionEnhanced_h_

#include <stdint.h>
#include "SamcoConst.h"

class SamcoPositionEnhanced {
  
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

    float angleOffset[4];

    int xx;
    int yy;

    unsigned int start = 0;

    unsigned int seenFlags = 0;
public:

    /// @brief Main function to calculate X, Y, and H
    void begin(const int* px, const int* py, unsigned int seen, int cx, int cy);
    
    int testX(int index) const { return FinalX[index]; }
    int testY(int index) const { return FinalY[index]; }
    unsigned int testSee(int index) const { return see[index]; }
    int testMedianX() const { return medianX; }
    int testMedianY() const { return medianY; }
    
    /// @brief X position
    int x() const { return xx; }
    
    /// @brief Y position
    int y() const { return yy; }
    
    /// @brief Height
    float h() const { return height; }
    
    /// @brief Bit mask of positions the camera saw
    unsigned int seen() const { return seenFlags; }
};

#endif // _SamcoPositionEnhanced_h_
