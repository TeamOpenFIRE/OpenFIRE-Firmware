/*!
 * @file OpenFIREConst.h
 * @brief Global constants for the OpenFIRE light gun.
 *
 * @copyright Mike Lynch, 2021
 * @copyright GNU Lesser General Public License
 *
 * @author Mike Lynch
 * @version V1.0
 * @date 2021
 */

#ifndef _OPENFIRECONST_H_
#define _OPENFIRECONST_H_

// DFRobot IR positioning camera resolution
constexpr int CamResX = 1024;
constexpr int CamResY = 768;

// DFRobot IR positioning camera maximum X and Y
constexpr int CamMaxX = CamResX - 1;
constexpr int CamMaxY = CamResY - 1;

// shift amount for extra precision for the maths
// since the median is an average of 4 values, use 2 more bits
constexpr int CamToMouseShift = 2;

// multiplier to convert IR camera position to mouse position
constexpr int CamToMouseMult = 1 << CamToMouseShift;

// mouse resolution
constexpr int MouseResX = CamResX * CamToMouseMult;
constexpr int MouseResY = CamResY * CamToMouseMult;

// Mouse position maximum X and Y
constexpr int MouseMaxX = MouseResX - 1;
constexpr int MouseMaxY = MouseResY - 1;

// Perspective code stuff
constexpr int res_x = 1920 << 2;
constexpr int res_y = 1080 << 2;

// DFRobot IR positioning camera update rate, in Hz.
// Must match the rate the sketch polls the camera at (startIrCamTimer()).
constexpr int CamFps = 209;

// Physical Square LED rectangle base / height ratio.
// Used by Square tracking (advanced tracking) as the initial shape guess.
constexpr float IRSquareBaseHeightRatio = 0.711f;

// Default Square LED positions in camera pixels, used until the first 4-LED lock:
// the largest IRSquareBaseHeightRatio rectangle in the middle 90% of the camera area.
constexpr int SquareDefaultTLX = 266;
constexpr int SquareDefaultTRX = 757;
constexpr int SquareDefaultTY = 38;
constexpr int SquareDefaultBY = 729;

// Default Diamond LED positions in camera pixels, used until the first 4-LED lock:
// the largest res_x:res_y diamond in the middle 90% of the camera area.
constexpr int DiamondDefaultTopX = 512;
constexpr int DiamondDefaultTopY = 124;
constexpr int DiamondDefaultRightX = 972;
constexpr int DiamondDefaultRightY = 384;
constexpr int DiamondDefaultBottomX = 512;
constexpr int DiamondDefaultBottomY = 643;
constexpr int DiamondDefaultLeftX = 51;
constexpr int DiamondDefaultLeftY = 384;

#endif // _OPENFIRECONST_H_
