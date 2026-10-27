/*
 * @file OpenFIRE_Perspective.h
 * @brief Light Gun library for 4 LED setup
 * @n CPP file for Samco Light Gun 4 LED setup
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

// The classic tracking is replaced by OpenFIRE_*_Advanced when ADVANCED_TRACKING is set
#ifndef ADVANCED_TRACKING


#ifndef OpenFIRE_Perspective_h
#define OpenFIRE_Perspective_h

class OpenFIRE_Perspective {

private:


  bool init = false;
  float srcmatrix[16];
  float dstmatrix[16];
  float warpmatrix[16];

  float dx0;
  float dy0;
  float dx1;
  float dy1; 
  float dx2; 
  float dy2; 
  float dx3; 
  float dy3;

  float srcX = 512.0f;
  float srcY = 384.0f;

  int dstX;
  int dstY;

public:
  void warp(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float dx0, float dy0, float dx1, float dy1, float dx2, float dy2, float dx3, float dy3);
  void source(float adjustedX, float adjustedY);
  void deinit (bool set);
  int getX();
  int getY();
};

#endif

#endif // !ADVANCED_TRACKING
