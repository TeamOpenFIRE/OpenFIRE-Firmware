/*!
 * @file OpenFIRE_Perspective_Advanced.h
 * @brief Advanced Light Gun library for 4 LED setup (perspective warp)
 * @n Replaces OpenFIRE_Perspective on FPU-equipped chips, see OpenFIRETracking.h
 *
 * Same maths as the classic warp, with 3x3 matrices, protection against degenerate input
 * (the last valid output is kept), lens correction of the LED and aim points, and rounding
 * instead of flooring.
 *
 * Modified from the OpenFIRE-Firmware-ESP32 port, 2026-10-24:
 *  - Ported to the RP2350, with the DFRobot camera values fixed as constants
 *    instead of per-camera profiles.
 *  - Renamed variables and methods, replaced magic numbers with named constants,
 *    and added explanatory comments.
 *
 * Based on the ESP32 port by:
 * @copyright alessandro-satanassi, https://github.com/alessandro-satanassi, 2026
 * @copyright GNU Lesser General Public License
 *
 * @author [Alessandro Satanassi](alessandro@cittini.it)
 * @version V2.0
 * @date 2026
 *
 * Based on the original code:
 *
 * @copyright Samco, https://github.com/samuelballantyne, 2024
 * @copyright GNU Lesser General Public License
 *
 * @author [Sam Ballantyne](samuelballantyne@hotmail.com)
 * @version V1.0
 * @date 2024
 * Derived from Wiimote Whiteboard library:
 * Copyright 2021 88hcsif
 * Copyright (c) 2008 Stephane Duchesneau
 * by Stephane Duchesneau <stephane.duchesneau@gmail.com>
 * Ported from Johnny Lee's C# WiiWhiteboard project (Warper.cs file)
 */

#include "OpenFIRETracking.h"

#ifdef ADVANCED_TRACKING

#ifndef _OpenFIRE_Perspective_Advanced_h_
#define _OpenFIRE_Perspective_Advanced_h_

#include "OpenFIREConst.h"

/// @details What this class does, in short.
///
///          The camera sees the four LEDs as a quadrilateral that changes shape with the gun's position and
///          angle: it grows when the player steps closer, leans over when the gun is rolled, and is a
///          trapezoid when the player stands to one side. On the screen those same four LEDs always form the same
///          fixed rectangle. A perspective warp (a homography, a 3x3 matrix) is the one transformation
///          that takes any such quadrilateral to the rectangle, and it also takes every other point of the camera
///          image along with it.
///
///          So the point the gun aims with (the aim point, a fixed point in the camera image) is warped with
///          the matrix of the current frame. The result is where on the screen the gun points, whatever the
///          position and angle of the player.
class OpenFIRE_Perspective {
public:
    /// @brief Works out where on the screen the aim point lands, for the LEDs seen in this frame.
    /// @param topLeftX,topLeftY,topRightX,topRightY,bottomLeftX,bottomLeftY,bottomRightX,bottomRightY
    ///        Positions of the four LEDs in the camera image, in mouse space
    /// @param destTopLeftX,destTopLeftY,destTopRightX,destTopRightY,destBottomLeftX,destBottomLeftY,destBottomRightX,destBottomRightY
    ///        Where the same four LEDs are on the screen, in screen units. They are fixed, so the matrix
    ///        built from them is only recomputed after deinit().
    /// @note  If the LEDs can't define a valid warp (too small, a line, ...), the previous result is kept.
    void warp(float topLeftX, float topLeftY, float topRightX, float topRightY,
              float bottomLeftX, float bottomLeftY, float bottomRightX, float bottomRightY,
              float destTopLeftX, float destTopLeftY, float destTopRightX, float destTopRightY,
              float destBottomLeftX, float destBottomLeftY, float destBottomRightX, float destBottomRightY);

    /// @brief Sets the aim point: the point of the camera image that the gun aims with.
    /// @note  It is not simply the centre of the image: calibration moves it to correct for how the camera is
    ///        mounted in the gun and for its roll.
    void source(float aimPointX, float aimPointY);

    /// @brief Sets whether the destination matrix is up to date. Passing false makes the next warp() rebuild it.
    void deinit(bool initialized);

    /// @brief X of where the gun aims on the screen, in screen units. Valid after warp().
    int getX();

    /// @brief Y of where the gun aims on the screen, in screen units. Valid after warp().
    int getY();

private:
    /// false until a warp() has built the destination matrix (and again after deinit(false))
    bool initialized = false;

    // The three matrices are 3x3 homographies, stored as 9 numbers row by row. They multiply a row
    // vector [x y 1] from the right.

    /// Camera image -> unit square. The inverse of the matrix that takes the unit square to the
    /// LEDs as the camera sees them in this frame.
    float sourceMatrix[9] = {};

    /// Unit square -> screen. Takes the unit square to the LEDs' fixed places on the screen.
    float destinationMatrix[9] = {};

    /// Camera image -> screen: the two matrices above combined into one, so a point needs only one multiplication.
    float warpMatrix[9] = {};

    /// The point of the camera image the gun aims with
    float aimPointX = (float)MouseResX * 0.5f;
    float aimPointY = (float)MouseResY * 0.5f;

    /// The last valid result, in screen units
    int screenX = 0;
    int screenY = 0;
};

#endif // _OpenFIRE_Perspective_Advanced_h_
#endif // ADVANCED_TRACKING
