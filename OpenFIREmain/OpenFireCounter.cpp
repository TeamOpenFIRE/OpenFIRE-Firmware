#include "OpenFireCounter.h"
#include <cctype> // para toupper

// --- "FUENTE" DE CARACTERES PRE-GIRADA 180° PARA HARDWARE INVERTIDO ---
const uint8_t OpenFireCounter::font[] = {
  // Números 0-9 (índices 0-9)
  0b11000000, // 0 
  0b11001111, // 1 
  0b10100100, // 2 
  0b10000110, // 3 
  0b10001001, // 4 
  0b10010010, // 5 
  0b10010000, // 6 
  0b10001111, // 7 
  0b10000000, // 8 
  0b10000010, // 9 
  // Letras Claras: A, b, C, d, E, F, H, I, L, O, P, S, U
  0b10001000, // A
  0b11100001, // b
  0b11000110, // C
  0b11000010, // d
  0b10000110, // E
  0b10001110, // F
  0b10001001, // H
  0b11001111, // I
  0b11000111, // L
  0b11000000, // O
  0b10001100, // P
  0b10010010, // S
  0b11000001, // U
  // Símbolos
  0b10011100, // grado
  0b10111111, // guion
  0b11110111, // bajo
  0b01111111, // punto
  // Caracter en blanco
  0b11111111
};


// Constructor (sin cambios)
OpenFireCounter::OpenFireCounter(spi_inst_t *spi_instance, uint sck_pin, uint mosi_pin, uint cs_pin)
    : _spi(spi_instance), _sck_pin(sck_pin), _mosi_pin(mosi_pin), _cs_pin(cs_pin) {}

// init() CON LA LÍNEA CRÍTICA LSB_FIRST
void OpenFireCounter::init() {
    spi_init(_spi, 1000 * 1000);
    
    // --- LÍNEA CRÍTICA ---
    // Configura el formato SPI a 8 bits y, lo más importante, LSB First.
    spi_set_format(_spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_LSB_FIRST);
    
    gpio_set_function(_sck_pin, GPIO_FUNC_SPI);
    gpio_set_function(_mosi_pin, GPIO_FUNC_SPI);
    gpio_init(_cs_pin);
    gpio_set_dir(_cs_pin, GPIO_OUT);
    gpio_put(_cs_pin, 1);
    print("HI");
}

// print() (sin cambios)
void OpenFireCounter::print(const std::string& text) {
    uint8_t patterns[2];
    patterns[0] = font[27]; 
    patterns[1] = font[27];
    int digit_index = 0;

    for (int i = 0; i < text.length() && digit_index < 2; i++) {
        char current_char = text[i];
        char next_char = (i + 1 < text.length()) ? text[i + 1] : '\0';

        if (current_char == '.') {
            patterns[digit_index] = getPattern('.');
            digit_index++;
        } else {
            patterns[digit_index] = getPattern(current_char);
            if (next_char == '.') {
                patterns[digit_index] &= getPattern('.');
                i++;
            }
            digit_index++;
        }
    }
    
    if (digit_index == 1) {
        bool is_numeric = false;
        if (!text.empty()) {
            is_numeric = (text[0] >= '0' && text[0] <= '9');
        }
        patterns[1] = patterns[0];
        if (is_numeric) {
            patterns[0] = getPattern('0');
        } else {
            patterns[0] = getPattern(' ');
        }
    }

    uint8_t buffer_to_send[2] = {patterns[0], patterns[1]};
    gpio_put(_cs_pin, 0);
    sleep_us(1);
    spi_write_blocking(_spi, buffer_to_send, 2);
    sleep_us(1);
    gpio_put(_cs_pin, 1);
}

// getPattern() (sin cambios)
uint8_t OpenFireCounter::getPattern(char c) {
  if (c >= '0' && c <= '9') {
    return font[c - '0'];
  }
  if (c == 'º') {
    return font[23];
  }
  char upper_c = toupper(c);
  switch (upper_c) {
    case 'A': return font[10];
    case 'B': return font[11]; // Muestra 'b'
    case 'C': return font[12];
    case 'D': return font[13]; // Muestra 'd'
    case 'E': return font[14];
    case 'F': return font[15];
    case 'H': return font[16];
    case 'I': return font[17];
    case 'L': return font[18];
    case 'O': return font[19];
    case 'P': return font[20];
    case 'S': return font[21];
    case 'U': return font[22];
    case '-': return font[24];
    case '_': return font[25];
    case '.': return font[26];
    case ' ': return font[27];
    default: return font[27]; 
  }
}
