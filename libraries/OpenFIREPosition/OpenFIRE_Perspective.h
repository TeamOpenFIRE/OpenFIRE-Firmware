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
  void warp(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, float dx0, float dy0, float dx1, float dy1, float dx2, float dy2, float dx3, float dy3);
  void source(float adjustedX, float adjustedY);
  void deinit (bool set);
  int getX();
  int getY();
};

#endif
