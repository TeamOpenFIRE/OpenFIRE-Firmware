 /*!
 * @file OpenFIREcounter.h
 * @Implementation of a 7-segment, 2-digit counter with shift registers based on the uhmxe-595-2 board for the OpenFIRE project.
 *
 * @copyright GustavoALara/PapaGustavoKratos, 2025
 * @copyright GNU Lesser General Public License
 */ 


#include "OpenFIREcounter.h"
#include "OpenFIREcommon.h"
#include <cctype>
#include <string>
#include <cstdio>

// --- STANDAR CHARACTER "FONT" (COMMON ANODE) ---
const uint8_t OpenFIREcounter::font[] = {
  // Numbers 0-9
  0b11000000, 0b11111001, 0b10100100, 0b10110000, 0b10011001,
  0b10010010, 0b10000010, 0b11111000, 0b10000000, 0b10010000,
  // Letters: A, b, C, d, E, F, G, H, I, J, L, O, P, S, U
  0b10001000, 0b10000011, 0b11000110, 0b10100001, 0b10000110,
  0b10001110, 0b11000010, 0b10001001, 0b11111001, 0b11110001,
  0b11000111, 0b11000000, 0b10001100, 0b10010010, 0b11000001,
  // Simbols: degree (*), hyphen (-), underscore (_), period (.)
  0b10011100, 0b10111111, 0b11110111, 0b01111111,
  // Blank character (space)
  0b11111111
};


// Constructor
OpenFIREcounter::OpenFIREcounter(spi_inst_t *spi_instance, uint sck_pin, uint mosi_pin, uint cs_pin)
    : _spi(spi_instance), _sck_pin(sck_pin), _mosi_pin(mosi_pin), _cs_pin(cs_pin) {}

// init() 
void OpenFIREcounter::init() {
    spi_init(_spi, 1000 * 1000);
    
    gpio_set_function(_sck_pin, GPIO_FUNC_SPI);
    gpio_set_function(_mosi_pin, GPIO_FUNC_SPI);
    gpio_init(_cs_pin);
    gpio_set_dir(_cs_pin, GPIO_OUT);
    gpio_put(_cs_pin, 1);
    
    // read custom message from settings
    uint16_t messagePacked = OF_Prefs::settings[OF_Const::counterStartupMessage];
    std::string startupMsg = "OF"; // Default message

    if (messagePacked != 0) {
        char msg[3];
        msg[0] = (messagePacked >> 8) & 0xFF; // first character
        msg[1] = messagePacked & 0xFF;        // second character
        msg[2] = '\0';
        startupMsg = msg;
    }
    
    print(startupMsg); // Muestra el mensaje de inicio
    // -------------------------
}

// print() with integrated sending logic
void OpenFIREcounter::print(const std::string& text) {
    uint8_t patterns[2];
    patterns[0] = font[29]; // Left digit in blank
    patterns[1] = font[29]; // Right digit in blank
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

    //  SPI logic sending
    uint8_t buffer_to_send[2] = {patterns[1], patterns[0]};
    gpio_put(_cs_pin, 0);
    sleep_us(1);
    spi_write_blocking(_spi, buffer_to_send, 2);
    sleep_us(1);
    gpio_put(_cs_pin, 1);
}


// getPattern() with full characters set
uint8_t OpenFIREcounter::getPattern(char c) {
  if (c >= '0' && c <= '9') {
    return font[c - '0'];
  }
  if (c == '*') {
    return font[25];
  }
  char upper_c = toupper(c);
  switch (upper_c) {
    case 'A': return font[10]; case 'B': return font[11];
    case 'C': return font[12]; case 'D': return font[13];
    case 'E': return font[14]; case 'F': return font[15];
    case 'G': return font[16]; case 'H': return font[17];
    case 'I': return font[18]; case 'J': return font[19];
    case 'L': return font[20]; case 'O': return font[21];
    case 'P': return font[22]; case 'S': return font[23];
    case 'U': return font[24]; case '-': return font[26];
    case '_': return font[27]; case '.': return font[28];
    case ' ': return font[29]; default: return font[29]; 
  }
}
