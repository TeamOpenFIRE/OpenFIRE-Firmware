/*!
 * @file OpenFIREcounter.h
 * @Implementation of a 7-segment, 2-digit counter with shift registers based on the uhmxe-595-2 board for the OpenFIRE project.
 *
 * @copyright GustavoALara/PapaGustavoKratos, 2025
 * @copyright GNU Lesser General Public License
 */ 

#ifndef OPENFIRE_COUNTER_H
#define OPENFIRE_COUNTER_H

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <string>

class OpenFIREcounter {
public:
    // Constructor
    OpenFIREcounter(spi_inst_t *spi_instance, uint sck_pin, uint mosi_pin, uint cs_pin);

    // Hardware initialization
    void init();

    // The main function that analyzes a string and displays it
    void print(const std::string& text);

private:
    // Returns the bit pattern for a given character
    uint8_t getPattern(char c);

    // Hardware variables
    spi_inst_t *_spi;
    uint _sck_pin;
    uint _mosi_pin;
    uint _cs_pin;
    
    // Character "Fonts"
    static const uint8_t font[];
};

#endif // OPENFIRE_COUNTER_H
