#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "inc/ssd1306.h"

#define LED_VERMELHO 13
#define LED_VERDE    11
#define BOTAO_A      5
#define BOTAO_B      6
#define BUZZER_A     21
#define BUZZER_B     10

const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

typedef enum { 
    ESTADO_VERMELHO, 
    ESTADO_VERDE, 
    ESTADO_AMARELO, 
    ESTADO_PEDESTRE_A, 
    ESTADO_PEDESTRE_B 
} estado_t;

volatile estado_t estado_atual = ESTADO_VERMELHO;
volatile bool pedido_A = false;
volatile bool pedido_B = false;
volatile int contagem_regressiva = 0;
volatile bool aguardando_fim_contagem = false;
volatile bool usar_buzzer_a = true;
volatile uint current_buzzer_gpio = BUZZER_A;

// --- Prototypes ---
bool timer_callback(repeating_timer_t *rt);
bool contagem_callback(repeating_timer_t *rt);


repeating_timer_t timer;
repeating_timer_t contagem_timer;
repeating_timer_t buzzer_timer;

void atualizar_display(const char* status, int contagem) {
    struct render_area area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&area);

    uint8_t buffer[ssd1306_buffer_length];
    memset(buffer, 0, ssd1306_buffer_length);

    // Exibir status
    ssd1306_draw_string(buffer, 0, 0, (char*)status);

    // Exibir contagem, se houver
    if (contagem > 0) {
        char countdown_str[16];
        sprintf(countdown_str, "Tempo: %d", contagem);
        ssd1306_draw_string(buffer, 0, 16, countdown_str);  // Segunda linha
    }

    render_on_display(buffer, &area);
}

bool buzzer_off_callback(repeating_timer_t *rt) {
    gpio_put(current_buzzer_gpio, 0);
    return false;
}

void buzzer_pulse() {
    current_buzzer_gpio = usar_buzzer_a ? BUZZER_A : BUZZER_B;
    usar_buzzer_a = !usar_buzzer_a;

    gpio_put(current_buzzer_gpio, 1);
    add_repeating_timer_ms(-200, buzzer_off_callback, NULL, &buzzer_timer);
}

void botao_callback(uint gpio, uint32_t events) {
    if (gpio == BOTAO_A) {
        pedido_A = true;
        printf("Botão A (Centro) acionado\n");
    } else if (gpio == BOTAO_B) {
        pedido_B = true;
        printf("Botão B (Bairro) acionado\n");
    }
}

void atualiza_semaforo(estado_t estado) {
    const char* display_status = "";

    switch (estado) {
        case ESTADO_VERMELHO:
            gpio_put(LED_VERMELHO, 1);
            gpio_put(LED_VERDE, 0);
            display_status = "Sinal: Vermelho";
            break;
        case ESTADO_VERDE:
            gpio_put(LED_VERMELHO, 0);
            gpio_put(LED_VERDE, 1);
            display_status = "Sinal: Verde";
            break;
        case ESTADO_AMARELO:
            gpio_put(LED_VERMELHO, 1);
            gpio_put(LED_VERDE, 1);
            display_status = "Sinal: Amarelo";
            break;
        case ESTADO_PEDESTRE_A:
            gpio_put(LED_VERMELHO, 1);
            gpio_put(LED_VERDE, 0);
            display_status = "Travessia: Centro";
            break;
        case ESTADO_PEDESTRE_B:
            gpio_put(LED_VERMELHO, 1);
            gpio_put(LED_VERDE, 0);
            display_status = "Travessia: Bairro";
            break;
    }

    atualizar_display(display_status, contagem_regressiva);
    printf("%s\n", display_status);
}

bool contagem_callback(repeating_timer_t *rt) {
    if (contagem_regressiva > 0) {
        printf("Contagem Regressiva: %d\n", contagem_regressiva);
        buzzer_pulse();

        atualizar_display(
            (estado_atual == ESTADO_PEDESTRE_A) ? "Travessia: Centro" : "Travessia: Bairro", 
            contagem_regressiva
        );
        contagem_regressiva--;
        return true;
    } else {
        aguardando_fim_contagem = false;
        add_repeating_timer_ms(-1, timer_callback, NULL, &timer);
        return false;
    }
}

bool timer_callback(repeating_timer_t *rt) {
    static bool em_travessia = false;

    if (aguardando_fim_contagem) return false;

    if (!em_travessia && (pedido_A || pedido_B)) {
        if (pedido_A) {
            estado_atual = ESTADO_AMARELO;
            atualiza_semaforo(ESTADO_AMARELO);
            em_travessia = true;
            pedido_A = false;
            estado_atual = ESTADO_PEDESTRE_A;
        } else if (pedido_B) {
            estado_atual = ESTADO_AMARELO;
            atualiza_semaforo(ESTADO_AMARELO);
            em_travessia = true;
            pedido_B = false;
            estado_atual = ESTADO_PEDESTRE_B;
        }
        add_repeating_timer_ms(-3000, timer_callback, NULL, &timer);
    } 
    else if (em_travessia) {
        atualiza_semaforo(estado_atual);

        add_repeating_timer_ms(-5000, timer_callback, NULL, &timer);

        contagem_regressiva = 5;
        aguardando_fim_contagem = true;
        usar_buzzer_a = true;
        add_repeating_timer_ms(-1000, contagem_callback, NULL, &contagem_timer);

        em_travessia = false;
    } 
    else {
        switch (estado_atual) {
            case ESTADO_VERMELHO:
                estado_atual = ESTADO_VERDE;
                atualiza_semaforo(ESTADO_VERDE);
                add_repeating_timer_ms(-10000, timer_callback, NULL, &timer);
                break;
            case ESTADO_VERDE:
                estado_atual = ESTADO_AMARELO;
                atualiza_semaforo(ESTADO_AMARELO);
                add_repeating_timer_ms(-3000, timer_callback, NULL, &timer);
                break;
            default:
                estado_atual = ESTADO_VERMELHO;
                atualiza_semaforo(ESTADO_VERMELHO);
                add_repeating_timer_ms(-10000, timer_callback, NULL, &timer);
                break;
        }
    }

    return false;
}

void setup_display() {
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init();

    struct render_area area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&area);

    uint8_t buffer[ssd1306_buffer_length];
    memset(buffer, 0, ssd1306_buffer_length);
    render_on_display(buffer, &area);
}

void setup_gpio() {
    gpio_init(LED_VERMELHO);
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);

    gpio_init(LED_VERDE);
    gpio_set_dir(LED_VERDE, GPIO_OUT);

    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);

    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);

    gpio_init(BUZZER_A);
    gpio_set_dir(BUZZER_A, GPIO_OUT);
    gpio_put(BUZZER_A, 0);

    gpio_init(BUZZER_B);
    gpio_set_dir(BUZZER_B, GPIO_OUT);
    gpio_put(BUZZER_B, 0);

    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &botao_callback);
    gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, true);
}

int main() {
    stdio_init_all();
    setup_gpio();
    setup_display();

    atualiza_semaforo(ESTADO_VERMELHO);
    add_repeating_timer_ms(-10000, timer_callback, NULL, &timer);

    while (true) {
        tight_loop_contents();
    }
}