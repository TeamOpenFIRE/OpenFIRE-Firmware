/*!
 * @file DFRobotIRPositionEx.h
 * @brief DFRobot's Positioning IR camera with extended functionality
 * @n Header file for DFRobot's Positioning IR camera
 * @details Extended functionality comes from http://wiibrew.org/wiki/Wiimote#IR_Camera
 * - Added basic data format, less IIC bytes than Extended
 * - Added size data to extended data format
 * - Added functions to atomically read the position data
 * - Added sensitivity settings
 * - Added IIC clock setting, appears to work up to 1MHz
 *
 * @copyright [DFRobot](http://www.dfrobot.com), 2016
 * @copyright Mike Lynch, 2021
 * @copyright GNU Lesser General Public License
 *
 * @author [Angelo](Angelo.qiao@dfrobot.com)
 * @author Mike Lynch
 * @version V1.0
 * @date 2021-07-14
 */

#ifndef DFRobotIRPositionEx_h
#define DFRobotIRPositionEx_h

#include <stdint.h>

/*!
*  @brief DFRobot IR positioning camera with extended functionality.
*/
class DFRobotIRPositionEx {
    const int IRAddress = 0xB0 >> 1; ///< IIC address of the sensor

    /*!
    * @brief Position data structure for a single extended frame.
    */
    typedef struct ExtendedFrame_s {
        uint8_t xLow;       ///< x position low byte
        uint8_t yLow;       ///< y position low byte
        uint8_t xyHighSize; ///< x and y position high byte and size
    } __attribute__ ((packed)) ExtendedFrame_t;

    /*!
    * @brief Position data structure for a single basic frame.
    */
    typedef struct BasicFrame_s {
        uint8_t x1low;  ///< x1 position low byte
        uint8_t y1low;  ///< y1 position low byte
        uint8_t high;   ///< packed high bits for both points
        uint8_t x2low;  ///< x2 position low byte
        uint8_t y2low;  ///< y2 position low byte
    } __attribute__ ((packed)) BasicFrame_t;

    /*!
    * @brief Position data structure for a single full frame
    */
    typedef struct FullFrame_s {
        uint8_t xLow;       ///< x position low byte
        uint8_t yLow;       ///< y position low byte
        uint8_t xyHighSize; ///< x and y position high byte and size
        uint8_t xMin;       ///< x minimum (7 bit value)
        uint8_t yMin;       ///< y minimum (7 bit value)
        uint8_t xMax;       ///< x maximum (7 bit value)
        uint8_t yMax;       ///< y maximum (7 bit value)
        uint8_t reserved;   ///< 0
        uint8_t intensity;  ///< 8 bit intensity value
    } __attribute__ ((packed)) FullFrame_t;

    /*!
    * @brief Position data structure to be filled from IIC data.
    */
    typedef union PositionData_u {
        uint8_t receivedBuffer[13]; ///< received buffer for IIC read
        struct {
            uint8_t header;
            union {
                ExtendedFrame_t rawExtended[4]; ///< 4 raw extended positions/frames.
                BasicFrame_t rawBasic[2];       ///< 2 raw basic frames.
            } __attribute__ ((packed)) format;
        } __attribute__ ((packed)) frame;
    }__attribute__ ((packed)) PositionData_t;  
 
    /*!
    * @brief Write two bytes into the sensor to initialize and send data.
    *
    * @param first the first byte
    * @param second the second byte
    */
    void writeTwoIICByte(uint8_t first, uint8_t second);

    /*!
    * @brief Request the position data. IIC will block the progress until all the data is recevied.
    */
    static bool readPosition(PositionData_t& posData, unsigned int length);

    /*!
    * @brief Unconditionally unpack basic frame from positionData. Does not update seen flags.
    */
   void unpackBasicFrame(unsigned int posData);

    /*!
    * @brief Unconditionally unpack extended frame from positionData. Does not update seen flags.
    */
   void unpackExtendedFrame(unsigned int posData);

    /*!
    * @brief Unpack basic frame from positionData and update position if seen. Seen flags are updated.
    */
   void unpackBasicFrameSeen(unsigned int posData);

    /*!
    * @brief Unpack extended frame from positionData and update position if seen. Seen flags are updated.
    */
   void unpackExtendedFrameSeen(unsigned int posData);

    /*!
    * @brief Raw postion data.
    */
    PositionData_t positionData[2];

    /*!
    * @brief Unpacked X positions.
    */
    int positionX[4];
    
    /*!
    * @brief Unpacked Y positions.
    */
    int positionY[4];
    
    /*!
    * @brief Unpacked sizes (when extended data format is used).
    */
    int unpackedSizes[4];

    /*!
    * @brief Bit mask of seen positions.
    */
    unsigned int seenFlags;

public:
  
    /*!
    * @brief Data format
    */
    enum DataFormat_e {
        DataFormat_Basic = 0,       ///< Basic data format.
        DataFormat_Extended = 1     ///< Extended data format that includes sizes.
        //DataFormat_Full = 3       ///< Full data format
    };

    /*!
    * @brief Camera sensitivity.
    * @details Sensitivity levels from http://wiibrew.org/wiki/Wiimote#IR_Camera
    */
    enum Sensitivity_e {
        Sensitivity_Min = 0,
        Sensitivity_Default = 0,  ///< default setting from the original library, suggested by "Marcan"
        Sensitivity_High = 1,     ///< high sensitivity, suggested by "inio"
        Sensitivity_Max = 2       ///< maximum sensitivity, suggested by "Kestrel"
    };

    /*!
    * @brief Error codes.
    * @details Overall success can be be checked by comparing to >= Error_Success (0).
    */
    enum Errors_e {
        Error_SuccessMismatch = 1,  ///< Data mismatch but using last frame data
        Error_Success = 0,        ///< Success
        Error_IICerror = -1,      ///< IIC error
        Error_DataMismatch = -2,  ///< Data mismatch
    };

    /*!
    * @brief Retry options for atomic read workaround.
    * @details The optimal setting is to use Retry_1s. If paranoid then use Retry_2. The other settings
    * are for advanced use cases if update time is liminited.
    */
    enum Retry_e {
        Retry_0 = 0,    ///< No retries, return Error_DataMismatch if mismatch
        Retry_0s = 1,   ///< No retries, if mismatch then use second frame and return Error_SuccessMismatch
        Retry_1 = 2,    ///< 1 retry, return Error_DataMismatch if mismatch
        Retry_1s = 3,   ///< 1 retry, optimal setting, if mismatch then use last frame and return Error_SuccessMismatch
        Retry_2 = 4,    ///< 2 retries, return Error_DataMismatch if mismatch
        Retry_2s = 5    ///< 2 retries, if mismatch then use last frame and return Error_SuccessMismatch
    };
    
    /*!
    * @brief Constructor
    */
    DFRobotIRPositionEx();
  
    /*!
    * @brief Destructor
    */
    ~DFRobotIRPositionEx();
  
    /*!
    * @brief Initialize the sensor.
    * @param[in] clock IIC clock rate. Defaults to 400kHz. Works up to at least 1MHz.
    * @param[in] format Initial data format. Defaults to basic.
    * @param[in] sensitivity Initial camera sensitivity.
    */
    void begin(uint32_t clock = 400000, DataFormat_e format = DataFormat_Basic, Sensitivity_e sensitivity = Sensitivity_Default);

    /*!
    * @brief Set the data format.
    */
    void dataFormat(DataFormat_e format);

    /*!
    * @brief Set the sensitivity.
    */
    void sensitivityLevel(Sensitivity_e sensitivity);

    /*!
    * @brief Request the extended position data that includes sizes.
    * @details You must set the format to DataFormat_Extended.
    */
    void requestPositionExtended();

    /*!
    * @brief Request the basic position data.
    * @details You must set the format to DataFormat_Basic.
    */
    void requestPositionBasic();

    /*!
    * @brief After requesting the extended position, and the data read from the sensor is ready, True will be returned.
    *
    * @return Whether data reading is ready.
    * @retval true Is ready
    * @retval false Is not ready
    */
    bool availableExtended();

    /*!
    * @brief Same as availableExtended() but uncondionally unpacks the positions and doesn't update the seen flags.
    * @details Useful if you want the unpacked data for analysis.
    *
    * @return Whether data reading is ready.
    * @retval true Is ready
    * @retval false Is not ready
    */
    bool availableExtendedNoSeen();

    /*!
    * @brief After requesting the basic position, and the data read from the sensor is ready, True will be returned.
    *
    * @return Whether data reading is ready.
    * @retval true Is ready
    * @retval false Is not ready
    */
    bool availableBasic();

    /*!
    * @brief Same as availableBasic() but uncondionally unpacks the positions and doesn't update the seen flags.
    * @details Useful if you want the unpacked data for analysis.
    *
    * @return Whether data reading is ready.
    * @retval true Is ready
    * @retval false Is not ready
    */
    bool availableBasicNoSeen();

    /*!
    * @brief Atomically update basic position data.
    * @details Since there is no aparent signalling to synchronize the read when the position updates,
    * this uses a workaround. The position data is read twice and compared. If it is the same then the
    * positions are updated. 2 retries will be a maximum of 4 readings and should guarentee success.
    * @param[in] retries Number of extra times to retry getting and matching the position.
    * @return An error code from Errors_e. 
    */
    int basicAtomic(DFRobotIRPositionEx::Retry_e retry = DFRobotIRPositionEx::Retry_1s);

    /*!
    * @brief Atomically update extended position data that includes the size.
    * @details Since there is no aparent signalling to synchronize the read when the position updates,
    * this uses a workaround. The position data is read twice and compared. If it is the same then the
    * positions are updated. 2 retries will be a maximum of 4 readings and should guarentee success.
    * @param[in] retries Number of extra times to retry getting and matching the position.
    * @return An error code from Errors_e. 
    */
    int extendedAtomic(DFRobotIRPositionEx::Retry_e retries = DFRobotIRPositionEx::Retry_1s);

    /*!
    * @brief Get the X position of a point.
    *
    * @param index The index of the 4 light objects ranging from 0 to 3.
    *
    * @return The X position corresponing to the index.
    */
    int x(int index) const { return positionX[index]; }

    /*!
    * @brief Get the Y position of a point.
    *
    * @param index The index of the 4 light objects ranging from 0 to 3.
    *
    * @return The Y position corresponing to the index.
    */
    int y(int index) const { return positionY[index]; }

    /*!
    * @brief Get the size of a point. This will be 15 if empty. Must use Extended data format.
    *
    * @param index The index of the 4 light objects ranging from 0 to 3,
    *
    * @return The size corresponing to the index.
    */
    int size(int index) const { return unpackedSizes[index]; }

    /*!
    * @brief Get the 4 X positions.
    *
    * @return Pointer to array of 4 X positions.
    */
    const int* xPositions() const { return positionX; }

    /*!
    * @brief Get the 4 Y positions.
    *
    * @return Pointer to array of 4 Y positions.
    */
    const int* yPositions() const { return positionY; }

    /*!
    * @brief Get the 4 sizes. Must use Extended data format.
    *
    * @return Pointer to array of 4 sizes.
    */
    const int* sizes() const { return unpackedSizes; }

    /*!
    * @brief Get seen bit mask. Bits 0 through 3 are set to 1 when a position is seen and updated.
    *
    * @return Seen flags.
    */
    unsigned int seen() const { return seenFlags; }
};

#endif // DFRobotIRPositionEx_h
