#include "OpenFireCounter.h"
#include <cctype> // para toupper

// Definimos la fuente como estática
const uint8_t CounterDisplay::font[] = {
    // Números 0-9 (índices 0-9)
    0b11000000, 0b11111001, 0b10100100, 0b10110000, 0b10011001, 0b10010010, 
    0b10000010, 0b11111000, 0b10000000, 0b10010000,
    // Letras Claras A, b, C, d, E, F, H, I, L, o, P, S, U (índices 10-22)
    0b10001000, 0b10000011, 0b11000110, 0b10100001, 0b10000110, 0b10001110, 
    0b10001001, 0b11111001, 0b11000111, 0b11000000, 0b10001100, 0b10010010, 
    0b11000001,
    // Símbolos: grado (°), guion (-), bajo (_), punto (.) (índices 23-26)
    0b10011100, 0b10111111, 0b11110111, 0b01111111,
    // Caracter en blanco (espacio) (índice 27)
    0b11111111,
    // Letras "parecidas" G, J, K, M, N, Q, R, T, V, W, X, Y, Z (índices 28-40)
    0b10000010, 0b11110001, 0b10001011, 0b11001010, 0b10101011, 0b10011000, 
    0b10101111, 0b10000111, 0b11100101, 0b10110101, 0b10001001, 0b10010001, 
    0b10100100
};

CounterDisplay::CounterDisplay(spi_inst_t *spi_instance, uint sck_pin, uint mosi_pin, uint cs_pin)
    : _spi(spi_instance), _sck_pin(sck_pin), _mosi_pin(mosi_pin), _cs_pin(cs_pin) {}

void CounterDisplay::init() {
    // Inicializa el SPI a 1MHz
    spi_init(_spi, 1000 * 1000);
    
    // Configura los pines GPIO para la función SPI
    gpio_set_function(_sck_pin, GPIO_FUNC_SPI);
    gpio_set_function(_mosi_pin, GPIO_FUNC_SPI);

    // Configura el pin de Latch (CS) como salida normal
    gpio_init(_cs_pin);
    gpio_set_dir(_cs_pin, GPIO_OUT);
    gpio_put(_cs_pin, 1); // Mantenerlo en alto por defecto

    // Muestra "HI" al iniciar
    print("HI");
}

void CounterDisplay::print(const std::string& text) {
    uint8_t patterns[2];
    patterns[0] = font[27]; // Dígito izquierdo en blanco
    patterns[1] = font[27]; // Dígito derecho en blanco
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
    
    // El orden de envío es {Dígito_Izquierdo, Dígito_Derecho}
    // porque el primer byte enviado termina en el último registro de la cadena.
    uint8_t buffer_to_send[2] = {patterns[0], patterns[1]};
    
    gpio_put(_cs_pin, 0); // Baja el Latch para empezar la transmisión
    sleep_us(1);
    spi_write_blocking(_spi, buffer_to_send, 2); // Envía los 2 bytes
    sleep_us(1);
    gpio_put(_cs_pin, 1); // Sube el Latch para mostrar los datos
}

uint8_t CounterDisplay::getPattern(char c) {
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
