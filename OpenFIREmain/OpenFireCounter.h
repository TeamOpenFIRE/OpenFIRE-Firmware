#ifndef COUNTER_DISPLAY_H
#define COUNTER_DISPLAY_H

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <string>

class CounterDisplay {
public:
    // Constructor: Define los pines SPI y de Latch (LOAD/CS)
    CounterDisplay(spi_inst_t *spi_instance, uint sck_pin, uint mosi_pin, uint cs_pin);

    // Inicializa el hardware SPI
    void init();

    // La función principal que analiza una cadena y la muestra
    void print(const std::string& text);

private:
    // Devuelve el patrón de bits para un caracter dado
    uint8_t getPattern(char c);

    // Variables de hardware
    spi_inst_t *_spi;
    uint _sck_pin;
    uint _mosi_pin; // En tu caso, MOSI es el pin SDI
    uint _cs_pin;   // El pin de Latch/LOAD
    
    // "Fuente" de caracteres
    static const uint8_t font[41];
};

#endif // COUNTER_DISPLAY_H
