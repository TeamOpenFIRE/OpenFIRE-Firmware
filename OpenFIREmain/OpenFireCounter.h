#ifndef OPENFIRE_COUNTER_H
#define OPENFIRE_COUNTER_H

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <string>

class OpenFireCounter {
public:
    // Constructor
    OpenFireCounter(spi_inst_t *spi_instance, uint sck_pin, uint mosi_pin, uint cs_pin);

    // Inicializa el hardware
    void init();

    // La función principal que analiza una cadena y la muestra
    void print(const std::string& text);

private:
    // Devuelve el patrón de bits para un caracter dado
    uint8_t getPattern(char c);

    // Variables de hardware
    spi_inst_t *_spi;
    uint _sck_pin;
    uint _mosi_pin;
    uint _cs_pin;
    
    // "Fuente" de caracteres
    static const uint8_t font[];
};

#endif // OPENFIRE_COUNTER_H
