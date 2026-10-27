/*!
 * @file OpenFIRE_Perspective_Advanced.cpp
 * @brief Advanced Light Gun library for 4 LED setup (perspective warp)
 * @n CPP file for Samco Light Gun 4 LED setup
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

#include <math.h>
#include "OpenFIRE_Perspective_Advanced.h"

namespace {

// Internal numeric scale. All positions are divided by it before the matrices are built, and the result
// is multiplied by it again. Positions are in the thousands, and the matrix maths multiplies and subtracts
// products of them; with floats (24 bits of precision) that loses digits. Scaled down to around 1,
// the same maths keeps much more of them. It is neither the camera's nor the monitor's resolution, and it
// is fixed so that the numerical behaviour stays the one that was validated with the DFRobot.
constexpr float NORM_SCALE = 10000.0f;
constexpr float INVERSE_NORM_SCALE = 1.0f / NORM_SCALE;

// The four LEDs must enclose at least this area to define a warp, as a fraction of the whole mouse space.
// For the DFRobot that is an area of 10 square mouse units: a speck far smaller than any real LED
// rectangle, so only degenerate input (a point or a line) falls below it.
constexpr float MIN_QUAD_AREA_RATIO = 7.947286e-7f;
constexpr float MIN_QUAD_AREA = (float)MouseResX * (float)MouseResY * MIN_QUAD_AREA_RATIO;

// Below this magnitude a determinant counts as zero: the equation system it belongs to has no usable solution.
constexpr float MIN_DETERMINANT = 1e-8f;

// Largest magnitude of the result before the conversion to int. Aiming almost parallel to the screen sends the
// projection towards infinity; this keeps the result inside the range of an int32 (up to about 2.1 billion).
constexpr float MAX_OUTPUT_COORDINATE = 2000000000.0f;

// Lens correction of the DFRobot camera. The lens bends straight lines slightly towards the edges of its
// image, which would distort the quadrilateral the warp is built from. Each point is moved away from
// the image centre by the factor 1 + K1 * r^2, where r is the distance from the centre, scaled so that it is
// 1 at the middle of the image sides. The factor is limited to the range below, so that wrong parameters
// or a point far outside the image can't deform the quadrilateral destructively.
constexpr float LENS_K1 = 0.006f;
constexpr float LENS_CORRECTION_MAX = 1.2f;
constexpr float LENS_CORRECTION_MIN = 0.8f;
constexpr float IMAGE_CENTER_X = (float)MouseResX * 0.5f;
constexpr float IMAGE_CENTER_Y = (float)MouseResY * 0.5f;
constexpr float INVERSE_IMAGE_CENTER_X = 1.0f / IMAGE_CENTER_X;
constexpr float INVERSE_IMAGE_CENTER_Y = 1.0f / IMAGE_CENTER_Y;

// Builds the matrix that takes the unit square to the quadrilateral of the four given points, which must be
// in order around the quadrilateral. The corners of the unit square (0,0), (1,0), (1,1) and (0,1) go to the
// first, second, third and fourth point.
//
// The matrix is a homography: a point (u, v) of the square goes to
//     x = (xFromU * u + xFromV * v + xOffset) / w
//     y = (yFromU * u + yFromV * v + yOffset) / w        with  w = perspectiveU * u + perspectiveV * v + 1
// The division by w is what makes it a perspective transformation and not just an affine one. The nine
// numbers are stored row by row, in the layout used to multiply a row vector [u v 1] from the right.
// This is the construction of Paul Heckbert's "Fundamentals of Texture Mapping and Image Warping".
//
// Returns false if the points can't define a valid homography. That is only checked when building the
// destination matrix, which is made once; the camera's matrix gets its own, lighter check.
bool computeSquareToQuad(float* matrix, float firstX, float firstY, float secondX, float secondY,
                         float thirdX, float thirdY, float fourthX, float fourthY)
{
    // The two edges that meet at the third point.
    const float thirdToSecondX = secondX - thirdX;
    const float thirdToSecondY = secondY - thirdY;
    const float thirdToFourthX = fourthX - thirdX;
    const float thirdToFourthY = fourthY - thirdY;

    // How far the quadrilateral is from a parallelogram, for which first + third = second + fourth. For a
    // parallelogram the perspective terms are zero and the warp is only affine.
    const float parallelogramErrorX = firstX - secondX + thirdX - fourthX;
    const float parallelogramErrorY = firstY - secondY + thirdY - fourthY;

    // The determinant of the two edges: zero if they are parallel, i.e. the second, third and fourth points
    // are on a line.
    const float determinant = (thirdToSecondX * thirdToFourthY - thirdToFourthX * thirdToSecondY);
    float perspectiveU = 0.0f, perspectiveV = 0.0f;

    // A zero determinant can't be solved for the perspective terms. A near-zero one is only acceptable if
    // the quadrilateral is a parallelogram (then no perspective terms are needed); a non-parallelogram
    // with it would need huge perspective terms, which is a collapsed shape, not a real view.
    if (!isfinite(determinant) || determinant == 0.0f ||
        (fabsf(determinant) <= MIN_DETERMINANT && (parallelogramErrorX != 0.0f || parallelogramErrorY != 0.0f)))
        return false;

    if (fabsf(determinant) > MIN_DETERMINANT) {
        // Solve the 2x2 system for the perspective terms. Multiplying by the inverse is cheaper than dividing twice.
        const float inverseDeterminant = 1.0f / determinant;
        perspectiveU = (parallelogramErrorX * thirdToFourthY - thirdToFourthX * parallelogramErrorY) * inverseDeterminant;
        perspectiveV = (thirdToSecondX * parallelogramErrorY - parallelogramErrorX * thirdToSecondY) * inverseDeterminant;
    }

    matrix[0] = secondX - firstX + perspectiveU * secondX;  // xFromU
    matrix[1] = secondY - firstY + perspectiveU * secondY;  // yFromU
    matrix[2] = perspectiveU;
    matrix[3] = fourthX - firstX + perspectiveV * fourthX;  // xFromV
    matrix[4] = fourthY - firstY + perspectiveV * fourthY;  // yFromV
    matrix[5] = perspectiveV;
    matrix[6] = firstX;                                     // xOffset
    matrix[7] = firstY;                                     // yOffset
    matrix[8] = 1.0f;

    // Don't hand out a matrix containing NaN or infinity as if it were valid.
    for (unsigned entry = 0; entry < 9; ++entry) {
        if (!isfinite(matrix[entry])) return false;
    }
    return true;
}

// The opposite of computeSquareToQuad(): builds the matrix that takes the quadrilateral of the four given points to
// the unit square. That is what lets the single pixel the camera reads (the aim point) be located relative to
// the LEDs, whatever shape they are seen in.
//
// It builds the unit-square-to-quadrilateral matrix in the same way, and then inverts it. The inverse of a 3x3 matrix
// is its adjugate (the transposed matrix of cofactors) divided by its determinant; the adjugate entries are written
// out below. Returns false if the matrix can't be inverted, so that the caller keeps its last valid result.
bool computeQuadToSquare(float* matrix, float firstX, float firstY, float secondX, float secondY,
                         float thirdX, float thirdY, float fourthX, float fourthY)
{
    const float thirdToSecondX = secondX - thirdX;
    const float thirdToSecondY = secondY - thirdY;
    const float thirdToFourthX = fourthX - thirdX;
    const float thirdToFourthY = fourthY - thirdY;
    const float parallelogramErrorX = firstX - secondX + thirdX - fourthX;
    const float parallelogramErrorY = firstY - secondY + thirdY - fourthY;

    const float determinant = (thirdToSecondX * thirdToFourthY - thirdToFourthX * thirdToSecondY);
    float perspectiveU = 0.0f, perspectiveV = 0.0f;

    // If the perspective terms can't be solved, but the quadrilateral isn't a parallelogram either, there is no
    // valid transformation.
    if (fabsf(determinant) <= MIN_DETERMINANT && (parallelogramErrorX != 0.0f || parallelogramErrorY != 0.0f))
        return false;

    if (fabsf(determinant) > MIN_DETERMINANT) {
        const float inverseDeterminant = 1.0f / determinant;
        perspectiveU = (parallelogramErrorX * thirdToFourthY - thirdToFourthX * parallelogramErrorY) * inverseDeterminant;
        perspectiveV = (thirdToSecondX * parallelogramErrorY - parallelogramErrorX * thirdToSecondY) * inverseDeterminant;
    }

    // The unit-square-to-quadrilateral matrix, entry by entry (see computeSquareToQuad()).
    const float xFromU = secondX - firstX + perspectiveU * secondX;
    const float xFromV = fourthX - firstX + perspectiveV * fourthX;
    const float xOffset = firstX;
    const float yFromU = secondY - firstY + perspectiveU * secondY;
    const float yFromV = fourthY - firstY + perspectiveV * fourthY;
    const float yOffset = firstY;

    // The adjugate of that matrix, named by row and column of the result.
    const float adjugate00 = yFromV - yOffset * perspectiveV;
    const float adjugate10 = xOffset * perspectiveV - xFromV;
    const float adjugate20 = xFromV * yOffset - xOffset * yFromV;
    const float adjugate01 = yOffset * perspectiveU - yFromU;
    const float adjugate11 = xFromU - xOffset * perspectiveU;
    const float adjugate21 = xOffset * yFromU - xFromU * yOffset;
    const float adjugate02 = yFromU * perspectiveV - yFromV * perspectiveU;
    const float adjugate12 = xFromV * perspectiveU - xFromU * perspectiveV;
    const float adjugate22 = xFromU * yFromV - xFromV * yFromU;

    // The determinant of the matrix, from its first column and the matching adjugate entries.
    const float matrixDeterminant = (xFromU * adjugate00 + xFromV * adjugate01 + xOffset * adjugate02);

    // A determinant of (nearly) zero means the LEDs are on a line or on top of each other, as when the camera
    // is covered or reports stray points. Fail explicitly: the caller then keeps the last valid output.
    if (fabsf(matrixDeterminant) <= MIN_DETERMINANT) return false;

    const float inverseMatrixDeterminant = 1.0f / matrixDeterminant;
    matrix[0] = adjugate00 * inverseMatrixDeterminant;
    matrix[1] = adjugate01 * inverseMatrixDeterminant;
    matrix[2] = adjugate02 * inverseMatrixDeterminant;
    matrix[3] = adjugate10 * inverseMatrixDeterminant;
    matrix[4] = adjugate11 * inverseMatrixDeterminant;
    matrix[5] = adjugate12 * inverseMatrixDeterminant;
    matrix[6] = adjugate20 * inverseMatrixDeterminant;
    matrix[7] = adjugate21 * inverseMatrixDeterminant;
    matrix[8] = adjugate22 * inverseMatrixDeterminant;
    return true;
}

// Multiplies two 3x3 matrices: result = first * second. Written out entry by entry instead of with loops,
// as this runs on every camera frame and the compiler can then schedule all of it freely.
inline void multiplyMatrices(const float* first, const float* second, float* result)
{
    result[0] = first[0]*second[0] + first[1]*second[3] + first[2]*second[6];
    result[1] = first[0]*second[1] + first[1]*second[4] + first[2]*second[7];
    result[2] = first[0]*second[2] + first[1]*second[5] + first[2]*second[8];
    result[3] = first[3]*second[0] + first[4]*second[3] + first[5]*second[6];
    result[4] = first[3]*second[1] + first[4]*second[4] + first[5]*second[7];
    result[5] = first[3]*second[2] + first[4]*second[5] + first[5]*second[8];
    result[6] = first[6]*second[0] + first[7]*second[3] + first[8]*second[6];
    result[7] = first[6]*second[1] + first[7]*second[4] + first[8]*second[7];
    result[8] = first[6]*second[2] + first[7]*second[5] + first[8]*second[8];
}

// Moves a point of the camera image to where it would be without the lens' distortion (a simplified
// Brown-Conrady radial model, see LENS_K1). Without it, the edges of the image would bend the LEDs' quadrilateral, and
// the warp calculated from it would be a little off wherever the LEDs are near the edge.
inline void applyLensCorrection(float& x, float& y)
{
    const float offsetX = x - IMAGE_CENTER_X;
    const float offsetY = y - IMAGE_CENTER_Y;

    // Distance from the centre, scaled so that the middle of an image side is at 1 on its axis.
    const float normalizedX = offsetX * INVERSE_IMAGE_CENTER_X;
    const float normalizedY = offsetY * INVERSE_IMAGE_CENTER_Y;

    const float squaredRadius = (normalizedX * normalizedX) + (normalizedY * normalizedY);
    float correction = 1.0f + (LENS_K1 * squaredRadius);

    // Limit the correction, so that wrong parameters can't deform the quadrilateral destructively.
    if (correction > LENS_CORRECTION_MAX) correction = LENS_CORRECTION_MAX;
    if (correction < LENS_CORRECTION_MIN) correction = LENS_CORRECTION_MIN;

    x = IMAGE_CENTER_X + (offsetX * correction);
    y = IMAGE_CENTER_Y + (offsetY * correction);
}

// Area of a quadrilateral with its corners in order around it: half the cross product of its two diagonals
// (the shoelace formula). Used to tell a real quadrilateral from a degenerate one (a point or a line).
inline float calculateQuadArea(float firstX, float firstY, float secondX, float secondY,
                               float thirdX, float thirdY, float fourthX, float fourthY)
{
    return 0.5f * fabsf((thirdX - firstX) * (fourthY - secondY) - (fourthX - secondX) * (thirdY - firstY));
}

} // namespace

void OpenFIRE_Perspective::warp(float topLeftX, float topLeftY, float topRightX, float topRightY,
                                float bottomLeftX, float bottomLeftY, float bottomRightX, float bottomRightY,
                                float destTopLeftX, float destTopLeftY, float destTopRightX, float destTopRightY,
                                float destBottomLeftX, float destBottomLeftY, float destBottomRightX, float destBottomRightY)
{
    // The matrix construction walks around the quadrilateral, so the LEDs have to be in order around it:
    // top left, top right, bottom right, bottom left. The callers give the bottom left before the bottom
    // right. In the wrong order the quadrilateral would cross over itself like an hourglass and the maths
    // would fall apart, so the order is fixed here, once.
    float perimeterX0 = topLeftX,     perimeterY0 = topLeftY;
    float perimeterX1 = topRightX,    perimeterY1 = topRightY;
    float perimeterX2 = bottomRightX, perimeterY2 = bottomRightY;
    float perimeterX3 = bottomLeftX,  perimeterY3 = bottomLeftY;

    // Before the lens correction, check that the four points still describe a real surface. A line or a point can't
    // define a warp, and the correction must not turn them into a quadrilateral that merely looks valid.
    const float rawArea = calculateQuadArea(perimeterX0, perimeterY0, perimeterX1, perimeterY1,
                                            perimeterX2, perimeterY2, perimeterX3, perimeterY3);
    if (rawArea <= MIN_QUAD_AREA) return;

    applyLensCorrection(perimeterX0, perimeterY0);
    applyLensCorrection(perimeterX1, perimeterY1);
    applyLensCorrection(perimeterX2, perimeterY2);
    applyLensCorrection(perimeterX3, perimeterY3);

    // The destination matrix describes where the LEDs are on the screen, which never changes, so it is
    // only built the first time (and after deinit(false)). The destination points are in the same order as above.
    if (!this->initialized) {
        const float correctedArea = calculateQuadArea(perimeterX0, perimeterY0, perimeterX1, perimeterY1,
                                                      perimeterX2, perimeterY2, perimeterX3, perimeterY3);
        if (correctedArea <= MIN_QUAD_AREA) return;

        if (!computeSquareToQuad(destinationMatrix, destTopLeftX * INVERSE_NORM_SCALE, destTopLeftY * INVERSE_NORM_SCALE,
                                                    destTopRightX * INVERSE_NORM_SCALE, destTopRightY * INVERSE_NORM_SCALE,
                                                    destBottomRightX * INVERSE_NORM_SCALE, destBottomRightY * INVERSE_NORM_SCALE,
                                                    destBottomLeftX * INVERSE_NORM_SCALE, destBottomLeftY * INVERSE_NORM_SCALE)) {
            return;
        }
        this->initialized = true;
    }

    perimeterX0 *= INVERSE_NORM_SCALE; perimeterY0 *= INVERSE_NORM_SCALE;
    perimeterX1 *= INVERSE_NORM_SCALE; perimeterY1 *= INVERSE_NORM_SCALE;
    perimeterX2 *= INVERSE_NORM_SCALE; perimeterY2 *= INVERSE_NORM_SCALE;
    perimeterX3 *= INVERSE_NORM_SCALE; perimeterY3 *= INVERSE_NORM_SCALE;

    // The matrix for this frame: camera image to unit square, given the LEDs as seen now. If it can't be built, the
    // previous result stays.
    if (!computeQuadToSquare(sourceMatrix, perimeterX0, perimeterY0, perimeterX1, perimeterY1,
                             perimeterX2, perimeterY2, perimeterX3, perimeterY3)) {
        return;
    }

    // Camera image to unit square to screen, as a single matrix.
    multiplyMatrices(sourceMatrix, destinationMatrix, warpMatrix);

    // The aim point is corrected for the lens in the same way as the LEDs were: both have to be in the same space.
    // The correction is applied to copies, so that it doesn't build up from frame to frame.
    float correctedAimX = aimPointX;
    float correctedAimY = aimPointY;
    applyLensCorrection(correctedAimX, correctedAimY);
    const float normalizedAimX = correctedAimX * INVERSE_NORM_SCALE;
    const float normalizedAimY = correctedAimY * INVERSE_NORM_SCALE;

    // Multiply the aim point [x y 1] by the warp matrix. The result is in homogeneous coordinates: the
    // third number is the w to divide by.
    const float projectedX = (normalizedAimX * warpMatrix[0] + normalizedAimY * warpMatrix[3] + warpMatrix[6]);
    const float projectedY = (normalizedAimX * warpMatrix[1] + normalizedAimY * warpMatrix[4] + warpMatrix[7]);
    const float projectedW = (normalizedAimX * warpMatrix[2] + normalizedAimY * warpMatrix[5] + warpMatrix[8]);

    // A w of zero means the aim point maps to infinity (the gun aims exactly parallel to the screen): no result.
    if (fabsf(projectedW) > MIN_DETERMINANT) {
        // The perspective division, back to two-dimensional screen coordinates, then undo the normalisation.
        const float inverseW = 1.0f / projectedW;
        float resultX = roundf((projectedX * inverseW) * NORM_SCALE);
        float resultY = roundf((projectedY * inverseW) * NORM_SCALE);

        // The clamp below doesn't catch NaN, since every comparison with it is false, so check it first.
        // With a result that isn't finite, both coordinates of the last valid output are kept.
        if (!isfinite(resultX) || !isfinite(resultY)) return;

        // Limit the magnitude before the cast to int, which is undefined if the value doesn't fit.
        if (resultX > MAX_OUTPUT_COORDINATE) resultX = MAX_OUTPUT_COORDINATE;
        if (resultX < -MAX_OUTPUT_COORDINATE) resultX = -MAX_OUTPUT_COORDINATE;
        if (resultY > MAX_OUTPUT_COORDINATE) resultY = MAX_OUTPUT_COORDINATE;
        if (resultY < -MAX_OUTPUT_COORDINATE) resultY = -MAX_OUTPUT_COORDINATE;

        this->screenX = (int)resultX;
        this->screenY = (int)resultY;
    }
}

void OpenFIRE_Perspective::source(float aimPointX, float aimPointY)
{
    this->aimPointX = aimPointX;
    this->aimPointY = aimPointY;
}

void OpenFIRE_Perspective::deinit(bool initialized)
{
    this->initialized = initialized;
}

int OpenFIRE_Perspective::getX()
{
    return screenX;
}

int OpenFIRE_Perspective::getY()
{
    return screenY;
}

#endif // ADVANCED_TRACKING
