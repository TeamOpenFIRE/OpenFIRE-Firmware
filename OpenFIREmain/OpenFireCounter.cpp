#include "OpenFireCounter.h"
#include <cctype>
#include <string>
#include <cstdio> // Para printf

// Usamos el último font que validaste como base
const uint8_t OpenFireCounter::font[] = {
  // Números 0-9
  0b11000000, 0b11001111, 0b10100100, 0b10000110, 0b10001011,
  0b10010010, 0b10010000, 0b11000111, 0b10000000, 0b10000010,
  // Letras y símbolos (estos pueden estar incorrectos hasta que tengamos el mapa)
  0b10001000, 0b10000011, 0b11000110, 0b10100001, 0b10000110,
  0b10001110, 0b10001001, 0b11111001, 0b11000111, 0b11000000,
  0b10001100, 0b10010010, 0b11000001, 0b10011100, 0b10111111,
  0b11110111, 0b01111111, 0b11111111
};


// Constructor (sin cambios)
OpenFireCounter::OpenFireCounter(spi_inst_t *spi_instance, uint sck_pin, uint mosi_pin, uint cs_pin)
    : _spi(spi_instance), _sck_pin(sck_pin), _mosi_pin(mosi_pin), _cs_pin(cs_pin) {}

// NUEVA FUNCIÓN PRIVADA para enviar datos
void OpenFireCounter::displayRawPatterns(uint8_t pattern_left, uint8_t pattern_right) {
    uint8_t buffer_to_send[2] = {pattern_left, pattern_right};
    gpio_put(_cs_pin, 0);
    sleep_us(1);
    spi_write_blocking(_spi, buffer_to_send, 2);
    sleep_us(1);
    gpio_put(_cs_pin, 1);
}

// init() AHORA CONTIENE EL TEST
void OpenFireCounter::init() {
    spi_init(_spi, 1000 * 1000);
    
    gpio_set_function(_sck_pin, GPIO_FUNC_SPI);
    gpio_set_function(_mosi_pin, GPIO_FUNC_SPI);
    gpio_init(_cs_pin);
    gpio_set_dir(_cs_pin, GPIO_OUT);
    gpio_put(_cs_pin, 1);
    
    // Espera un poco para dar tiempo a abrir el monitor serie
    sleep_ms(3000); 
    printf("\n\n--- Iniciando test de segmentos del contador ---\n");

    for (int i = 0; i < 8; i++) {
        // Genera un patrón para encender un solo bit (0=ON para ánodo común)
        uint8_t pattern = ~(1 << i);

        printf("Probando Bit %d (Patrón: ", i);
        for (int b = 7; b >= 0; b--) {
            printf("%d", (pattern >> b) & 1);
        }
        printf(")\n");
        
        // Muestra el patrón en ambos dígitos
        displayRawPatterns(pattern, pattern);
        
        sleep_ms(4000); // 4 segundos para observar con calma
    }
    
    printf("\n--- Test finalizado. El display mostrará '--' ---\n\n");
    // Dejamos un estado final en el display
    print("--"); 
}

// print() ahora usa la nueva función privada
void OpenFireCounter::print(const std::string& text) {
    uint8_t patterns[2];
    patterns[0] = font[27]; 
    patterns[1] = font[27];
    int digit_index = 0;
    // ... (la lógica de análisis de la cadena no cambia) ...
    for (int i = 0; i < text.length() && digit_index < 2; i++) {
        char current_char = text[i];
        char next_char = (i + 1 < text.length()) ? text[i + 1] : '\0';
        if (current_char == '.') { patterns[digit_index] = getPattern('.'); digit_index++; } 
        else {
            patterns[digit_index] = getPattern(current_char);
            if (next_char == '.') { patterns[digit_index] &= getPattern('.'); i++; }
            digit_index++;
        }
    }
    if (digit_index == 1) {
        bool is_numeric = false;
        if (!text.empty()) { is_numeric = (text[0] >= '0' && text[0] <= '9'); }
        patterns[1] = patterns[0];
        if (is_numeric) { patterns[0] = getPattern('0'); }
        else { patterns[0] = getPattern(' '); }
    }
    // Llama a la nueva función para enviar los datos
    displayRawPatterns(patterns[0], patterns[1]);
}

// getPattern() (lógica sin cambios)
uint8_t OpenFireCounter::getPattern(char c) {
    // ... (el contenido del switch no cambia) ...
    if (c >= '0' && c <= '9') { return font[c - '0']; }
    if (c == '*') { return font[23]; }
    char upper_c = toupper(c);
    switch (upper_c) {
        case 'A': return font[10]; case 'B': return font[11];
        case 'C': return font[12]; case 'D': return font[13];
        case 'E': return font[14]; case 'F': return font[15];
        case 'H': return font[16]; case 'I': return font[17];
        case 'L': return font[18]; case 'O': return font[19];
        case 'P': return font[20]; case 'S': return font[21];
        case 'U': return font[22]; case '-': return font[24];
        case '_': return font[25]; case '.': return font[26];
        case ' ': return font[27]; default: return font[27];
    }
}
