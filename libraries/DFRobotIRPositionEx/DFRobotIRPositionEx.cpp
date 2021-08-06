/*!
 * @file DFRobotIRPositionEx.cpp
 * @brief DFRobot's Positioning IR camera with extended functionality
 * @n CPP file for DFRobot's Positioning IR camera
 * @details Extended functionality comes from http://wiibrew.org/wiki/Wiimote#IR_Camera
 * - Added basic data format, less IIC bytes than Extended
 * - Added size data to extended data format
 * - Added functions to atomically read the position data
 * - Added sensitivity settings
 * - Added IIC clock setting, appears to work up to at least 1MHz
 *
 * @copyright [DFRobot](http://www.dfrobot.com), 2016
 * @copyright Mike "Prow" Lynch, 2021
 * @copyright GNU Lesser General Public License
 *
 * @author [Angelo](Angelo.qiao@dfrobot.com)
 * @author Mike Lynch
 * @version V1.0
 * @date 2021-07-14
 */

#include <Arduino.h>
#include <Wire.h>
#include "DFRobotIRPositionEx.h"

// data lengths
constexpr unsigned int DFRIRdata_LengthBasic = 11;
constexpr unsigned int DFRIRdata_LengthExtended = 13;
constexpr unsigned int DFRIRdata_LengthFull = 9 * 4 + 1;

// data format mode register values
constexpr uint8_t DFRIRdata_ModeBasic = 0x11;
constexpr uint8_t DFRIRdata_ModeExtended = 0x33;
constexpr uint8_t DFRIRdata_ModeFull = 0x55;

// IIC delay, the Wiki says to use at least 50ms, but the original source uses 10
constexpr unsigned long DFRIRdata_IICdelay = 10;

// maximum valid Y position
constexpr int DFRIRdata_MaxY = 767;

DFRobotIRPositionEx::DFRobotIRPositionEx() : seenFlags(0)
{
}

DFRobotIRPositionEx::~DFRobotIRPositionEx()
{
}

void DFRobotIRPositionEx::writeTwoIICByte(uint8_t first, uint8_t second)
{
    Wire.beginTransmission(IRAddress);
    Wire.write(first);
    Wire.write(second);
    Wire.endTransmission();
}

void DFRobotIRPositionEx::dataFormat(DataFormat_e format)
{
    uint8_t mode = format ? DFRIRdata_ModeExtended : DFRIRdata_ModeBasic;
    writeTwoIICByte(0x33, mode);
    delay(DFRIRdata_IICdelay);
}

void DFRobotIRPositionEx::sensitivityLevel(Sensitivity_e sensitivity)
{
    // sensitivity data from http://wiibrew.org/wiki/Wiimote#IR_Camera
    static const uint8_t reg06[3] = {0x90, 0x90, 0xFF};
    static const uint8_t reg08[3] = {0xC0, 0x41, 0x0C};
    static const uint8_t reg1A[3] = {0x40, 0x40, 0x00};
    if(sensitivity > Sensitivity_Max) {
        sensitivity = Sensitivity_Max;
    }
    writeTwoIICByte(0x06, reg06[sensitivity]);
    delay(DFRIRdata_IICdelay);
    writeTwoIICByte(0x08, reg08[sensitivity]);
    delay(DFRIRdata_IICdelay);
    writeTwoIICByte(0x1A, reg1A[sensitivity]);
    delay(DFRIRdata_IICdelay);
}

void DFRobotIRPositionEx::begin(uint32_t clock, DataFormat_e format, Sensitivity_e sensitivity)
{
    Wire.begin();
    // looking under the covers, the Wire default is only 100kHz (on ItsyBitsy SAMD), so allow a custom setting
    Wire.setClock(clock);
    // stop camera?
    writeTwoIICByte(0x30,0x01);
    delay(DFRIRdata_IICdelay);
    sensitivityLevel(sensitivity);
    dataFormat(format);
    // start camera?
    writeTwoIICByte(0x30,0x08);

    delay(100);
}

void DFRobotIRPositionEx::requestPositionExtended()
{
    Wire.beginTransmission(IRAddress);
    Wire.write(0x36);
    Wire.endTransmission();
    Wire.requestFrom(IRAddress, DFRIRdata_LengthExtended);
}

void DFRobotIRPositionEx::requestPositionBasic()
{
    Wire.beginTransmission(IRAddress);
    Wire.write(0x36);
    Wire.endTransmission();
    Wire.requestFrom(IRAddress, DFRIRdata_LengthBasic);
}

bool DFRobotIRPositionEx::availableExtended()
{
    if(readPosition(positionData[0], DFRIRdata_LengthExtended)) {
        unpackExtendedFrameSeen(0);
        return true;
    }
    return false;
}

bool DFRobotIRPositionEx::availableExtendedNoSeen()
{
    if(readPosition(positionData[0], DFRIRdata_LengthExtended)) {
        unpackExtendedFrame(0);
        return true;
    }
    return false;
}

bool DFRobotIRPositionEx::availableBasic()
{
    if(readPosition(positionData[0], DFRIRdata_LengthBasic)) {
        unpackBasicFrameSeen(0);
        return true;
    }
    return false;
}

bool DFRobotIRPositionEx::availableBasicNoSeen()
{
    if(readPosition(positionData[0], DFRIRdata_LengthBasic)) {
        unpackBasicFrame(0);
        return true;
    }
    return false;
}

bool DFRobotIRPositionEx::readPosition(PositionData_t& posData, unsigned int length)
{
    if(Wire.available() == length) {   //read only the data lenth fits.
        for(int i = 0; i < length; ++i) {
            posData.receivedBuffer[i] = Wire.read();
        }

        // looks like the header should always be 0, extra sanity for valid data
        //if(posData.positionFrame.header != 0) {
        //    return false;
        //}
        return true;
    }

    // length mismatch, flush the read buffer
    while(Wire.available()) {
        Wire.read();
    }
    return false;
}

void DFRobotIRPositionEx::unpackBasicFrame(unsigned int posData)
{
    BasicFrame_t& frame = positionData[posData].frame.format.rawBasic[0];
    int high = frame.high;
    positionX[0] = (int)frame.x1low | ((high & 0x30) << 4);
    positionY[0] = (int)frame.y1low | ((high & 0xC0) << 2);
    positionX[1] = (int)frame.x2low | ((high & 0x03) << 8);
    positionY[1] = (int)frame.y2low | ((high & 0x0C) << 6);

    frame = positionData[posData].frame.format.rawBasic[1];
    high = frame.high;
    positionX[2] = (int)frame.x1low | ((high & 0x30) << 4);
    positionY[2] = (int)frame.y1low | ((high & 0xC0) << 2);
    positionX[3] = (int)frame.x2low | ((high & 0x03) << 8);
    positionY[3] = (int)frame.y2low | ((high & 0x0C) << 6);
}

void DFRobotIRPositionEx::unpackBasicFrameSeen(unsigned int posData)
{
    seenFlags = 0;
    BasicFrame_t& frame = positionData[posData].frame.format.rawBasic[0];
    int high = frame.high;
    int y = (int)frame.y1low | ((high & 0xC0) << 2);
    if(y <= DFRIRdata_MaxY) {
        positionY[0] = y;
        positionX[0] = (int)frame.x1low | ((high & 0x30) << 4);
        seenFlags |= 0x01;
    }
    y = (int)frame.y2low | ((high & 0x0C) << 6);
    if(y <= DFRIRdata_MaxY) {
        positionY[1] = y;
        positionX[1] = (int)frame.x2low | ((high & 0x03) << 8);
        seenFlags |= 0x02;
    }

    frame = positionData[posData].frame.format.rawBasic[1];
    high = frame.high;
    y = (int)frame.y1low | ((high & 0xC0) << 2);
    if(y <= DFRIRdata_MaxY) {
        positionY[2] = y;
        positionX[2] = (int)frame.x1low | ((high & 0x30) << 4);
        seenFlags |= 0x04;
    }
    y = (int)frame.y2low | ((high & 0x0C) << 6);
    if(y <= DFRIRdata_MaxY) {
        positionY[3] = y;
        positionX[3] = (int)frame.x2low | ((high & 0x03) << 8);
        seenFlags |= 0x08;
    }
}

int DFRobotIRPositionEx::basicAtomic(DFRobotIRPositionEx::Retry_e retry)
{
    // initial index for positiondata[1]
    unsigned int index = 0;
    
    // initial read in positiondata[0]
    requestPositionBasic();
    if(!readPosition(positionData[0], DFRIRdata_LengthBasic)) {
        return Error_IICerror;
    }

    for(unsigned int i = 0, retries = retry >> 1; i <= retries; ++i) {
        requestPositionBasic();

        // switch to other buffer for next read
        index ^= 1;

        if(!readPosition(positionData[index], DFRIRdata_LengthBasic)) {
            return Error_IICerror;
        }

        // compare but ignore the header byte
        if(!memcmp(&positionData[0].receivedBuffer[1], &positionData[1].receivedBuffer[1], DFRIRdata_LengthBasic - 1)) {
            // position data is identical so unpack the data
            unpackBasicFrameSeen(0);
            return Error_Success;
        }
    }
    
    if(retry & 1) {
        unpackBasicFrameSeen(index);
        return Error_SuccessMismatch;
    }

    return Error_DataMismatch;
}

void DFRobotIRPositionEx::unpackExtendedFrame(unsigned int posData)
{
    for(int i = 0; i < 4; ++i) {
        ExtendedFrame_t& frame = positionData[posData].frame.format.rawExtended[i];
        positionX[i] = (int)frame.xLow | ((int)(frame.xyHighSize & 0x30U) << 4);
        positionY[i] = (int)frame.yLow | ((int)(frame.xyHighSize & 0xC0U) << 2);
        unpackedSizes[i] = frame.xyHighSize & 0xF;
    }
}

void DFRobotIRPositionEx::unpackExtendedFrameSeen(unsigned int posData)
{
    seenFlags = 0;
    for(int i = 0; i < 4; ++i) {
        ExtendedFrame_t& frame = positionData[posData].frame.format.rawExtended[i];
        int y = (int)frame.yLow | ((int)(frame.xyHighSize & 0xC0U) << 2);
        if(y <= DFRIRdata_MaxY) {
            positionY[i] = y;
            positionX[i] = (int)frame.xLow | ((int)(frame.xyHighSize & 0x30U) << 4);
            unpackedSizes[i] = frame.xyHighSize & 0xF;
            seenFlags |= 1 << i;
        }
    }
}

int DFRobotIRPositionEx::extendedAtomic(DFRobotIRPositionEx::Retry_e retry)
{
    // initial index for positiondata[1]
    unsigned int index = 0;
    
    // initial read in positiondata[0]
    requestPositionExtended();
    if(!readPosition(positionData[0], DFRIRdata_LengthExtended)) {
        return Error_IICerror;
    }

    for(unsigned int i = 0, retries = retry >> 1; i <= retries; ++i) {
        requestPositionExtended();

        // switch to other buffer for next read
        index ^= 1;

        if(!readPosition(positionData[index], DFRIRdata_LengthExtended)) {
            return Error_IICerror;
        }

        // compare but ignore the header byte
        if(!memcmp(&positionData[0].receivedBuffer[1], &positionData[1].receivedBuffer[1], DFRIRdata_LengthExtended - 1)) {
            // position data is identical so unpack the data
            unpackExtendedFrameSeen(index);
            return Error_Success;
        }
    }

    if(retry & 1) {
        unpackExtendedFrameSeen(index);
        return Error_SuccessMismatch;
    }

    return Error_DataMismatch;
}
