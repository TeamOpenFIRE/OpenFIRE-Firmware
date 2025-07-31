#include "OpenFireCounter.h"
#include <cctype>
#include <string>
#include <cstdio>

// --- "FUENTE" DE CARACTERES ESTÁNDAR DEFINITIVA (ÁNODO COMÚN) ---
// Basada en el mapa de pines 100% estándar que hemos confirmado con tu test.
const uint8_t OpenFireCounter::font[] = {
  // Números 0-9
  0b11000000, // 0
  0b11111001, // 1
  0b10100100, // 2
  0b10110000, // 3
  0b10011001, // 4
  0b10010010, // 5
  0b10000010, // 6
  0b11111000, // 7
  0b10000000, // 8
  0b10010000, // 9
  // Letras: A, b, C, d, E, F, G, H, I, J, L, O, P, S, U
  0b10001000, // A (índice 10)
  0b10000011, // b
  0b11000110, // C
  0b10100001, // d
  0b10000110, // E
  0b10001110, // F
  0b11000010, // G (es como un 0 con la barra superior)
  0b10001001, // H
  0b11111001, // I (igual que el 1)
  0b11110001, // J
  0b11000111, // L
  0b11000000, // O (igual que el 0)
  0b10001100, // P
  0b10010010, // S (igual que el 5)
  0b11000001, // U
  // Símbolos: grado (*), guion (-), bajo (_), punto (.)
  0b10011100, // grado (*) (índice 25)
  0b10111111, // guion (-)
  0b11110111, // bajo (_)
  0b01111111, // punto (.)
  // Caracter en blanco (espacio)
  0b11111111 // (índice 29)
};


// Constructor
OpenFireCounter::OpenFireCounter(spi_inst_t *spi_instance, uint sck_pin, uint mosi_pin, uint cs_pin)
    : _spi(spi_instance), _sck_pin(sck_pin), _mosi_pin(mosi_pin), _cs_pin(cs_pin) {}


// NUEVA: Función privada para enviar los patrones
void OpenFireCounter::displayRawPatterns(uint8_t pattern_left, uint8_t pattern_right) {
    uint8_t buffer_to_send[2] = {pattern_left, pattern_right};
    gpio_put(_cs_pin, 0);
    sleep_us(1);
    spi_write_blocking(_spi, buffer_to_send, 2);
    sleep_us(1);
    gpio_put(_cs_pin, 1);
}

// init() - Versión final sin el bucle de prueba
void OpenFireCounter::init() {
    spi_init(_spi, 1000 * 1000);
    // NO se necesita spi_set_format, el modo por defecto (MSB_FIRST) es el correcto.
    
    gpio_set_function(_sck_pin, GPIO_FUNC_SPI);
    gpio_set_function(_mosi_pin, GPIO_FUNC_SPI);
    gpio_init(_cs_pin);
    gpio_set_dir(_cs_pin, GPIO_OUT);
    gpio_put(_cs_pin, 1);
    
    print("88"); // Mensaje de inicio
}

// print() con la lógica de formateo
void OpenFireCounter::print(const std::string& text) {
    uint8_t patterns[2];
    patterns[0] = font[29]; // Dígito izquierdo en blanco por defecto
    patterns[1] = font[29]; // Dígito derecho en blanco por defecto
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

    displayRawPatterns(patterns[0], patterns[1]);
}


// getPattern() con el set de caracteres completo
uint8_t OpenFireCounter::getPattern(char c) {
  if (c >= '0' && c <= '9') {
    return font[c - '0'];
  }
  
  if (c == '*') {
    return font[25];
  }

  char upper_c = toupper(c);
  switch (upper_c) {
    case 'A': return font[10];
    case 'B': return font[11];
    case 'C': return font[12];
    case 'D': return font[13];
    case 'E': return font[14];
    case 'F': return font[15];
    case 'G': return font[16];
    case 'H': return font[17];
    case 'I': return font[18];
    case 'J': return font[19];
    case 'L': return font[20];
    case 'O': return font[21];
    case 'P': return font[22];
    case 'S': return font[23];
    case 'U': return font[24];
    case '-': return font[26];
    case '_': return font[27];
    case '.': return font[28];
    case ' ': return font[29];
    default: return font[29]; 
  }
}
