#include "OpenFireCounter.h"
#include <cctype> // para toupper

// Definimos la fuente como estática
const uint8_t OpenFireCounter::font[] = {
  // Números 0-9 (índices 0-9) 
  0b00000011, 0b10011111, 0b00100101, 0b00001101, 0b10011001, 
  0b01001001, 0b01000001, 0b00011111, 0b00000001, 0b00011001,
  // Letras Claras A, b, C, d, E, F, H, I, L, O, P, S, U (índices 10-22) 
  0b00010001, 0b11000001, 0b01100011, 0b10000101, 0b01100001, 0b01110001, 
  0b10010001, 0b10011111, 0b01110011, 0b00000011, 0b00110001, 0b01001001, 
  0b10000011,
  // Símbolos: grado (°), guion (-), bajo (_), punto (.) (índices 23-26) 
  0b00111001, 0b11111101, 0b11110111, 0b11111110,
  // Caracter en blanco (espacio) (índice 27) 
  0b11111111
};

OpenFireCounter::OpenFireCounter(spi_inst_t *spi_instance, uint sck_pin, uint mosi_pin, uint cs_pin)
    : _spi(spi_instance), _sck_pin(sck_pin), _mosi_pin(mosi_pin), _cs_pin(cs_pin) {}

void OpenFireCounter::init() {
    // Inicializa el SPI a 1MHz
    spi_init(_spi, 1000 * 1000);
    
    // Configura los pines GPIO para la función SPI
    gpio_set_function(_sck_pin, GPIO_FUNC_SPI);
    gpio_set_function(_mosi_pin, GPIO_FUNC_SPI);

    // Configura el pin de Latch (CS) como salida normal
    gpio_init(_cs_pin);
    gpio_set_dir(_cs_pin, GPIO_OUT);
    gpio_put(_cs_pin, 1); // Mantenerlo en alto por defecto

    // Muestra "OF" al iniciar
    print("OF");
}

// En src/OpenFireCounter.cpp

void OpenFireCounter::print(const std::string& text) {
    uint8_t patterns[2];
    patterns[0] = font[27]; // Dígito izquierdo en blanco por defecto
    patterns[1] = font[27]; // Dígito derecho en blanco por defecto
    int digit_index = 0;

    // Lógica de análisis de la cadena 
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
    
    // LÓGICA DE FORMATEO ---
    // Si solo hemos procesado un caracter (un dígito o una letra)
    if (digit_index == 1) {
        bool is_numeric = false;
        if (!text.empty()) {
            // Comprobamos si el primer caracter de la cadena es un número
            is_numeric = (text[0] >= '0' && text[0] <= '9');
        }
        
        // Movemos el caracter al dígito de la derecha (patterns[1])
        patterns[1] = patterns[0]; 
        
        if (is_numeric) {
            patterns[0] = getPattern('0'); // Añadimos un cero a la izquierda
        } else {
            patterns[0] = getPattern(' '); // Añadimos un espacio a la izquierda
        }
    }
   

    // Envío a los registros 
    uint8_t buffer_to_send[2] = {patterns[0], patterns[1]};
    gpio_put(_cs_pin, 0);
    sleep_us(1);
    spi_write_blocking(_spi, buffer_to_send, 2);
    sleep_us(1);
    gpio_put(_cs_pin, 1);
}

uint8_t OpenFireCounter::getPattern(char c) {
    // La lógica de esta función es idéntica a la versión de Arduino
    if (c >= '0' && c <= '9') {
        return font[c - '0'];
    }
    if (c == 'º') {
        return font[23];
    }
    char upper_c = std::toupper(c);
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
      default: return font[27]; // Espacio para caracteres no reconocidos
    }
}
