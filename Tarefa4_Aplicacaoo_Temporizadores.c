#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include <stdio.h>

#define LED_VERMELHO 13
#define LED_VERDE    11
#define BOTAO_PEDE   5
#define BUZZER_A     21
#define BUZZER_B     10

typedef enum { 
    ESTADO_VERMELHO, 
    ESTADO_VERDE, 
    ESTADO_AMARELO, 
    ESTADO_PEDESTRE 
} estado_t;

volatile estado_t estado_atual = ESTADO_VERMELHO;
volatile bool botao_pressionado = false;
volatile int contagem_regressiva = 0;
volatile bool aguardando_fim_contagem = false;
volatile bool usar_buzzer_a = true;  // Alternador de buzzers
volatile uint current_buzzer_gpio = BUZZER_A;

// --- Prototypes ---
bool timer_callback(repeating_timer_t *rt);
bool contagem_callback(repeating_timer_t *rt);

repeating_timer_t timer;
repeating_timer_t contagem_timer;
repeating_timer_t buzzer_timer;

void botao_callback(uint gpio, uint32_t events) {
    botao_pressionado = true;
    printf("Botão de Pedestres acionado\n");
}

void atualiza_semaforo(estado_t estado) {
    switch (estado) {
        case ESTADO_VERMELHO:
            gpio_put(LED_VERMELHO, 1);
            gpio_put(LED_VERDE, 0);
            printf("Sinal: Vermelho\n");
            break;
        case ESTADO_VERDE:
            gpio_put(LED_VERMELHO, 0);
            gpio_put(LED_VERDE, 1);
            printf("Sinal: Verde\n");
            break;
        case ESTADO_AMARELO:
            gpio_put(LED_VERMELHO, 1);
            gpio_put(LED_VERDE, 1);
            printf("Sinal: Amarelo\n");
            break;
        case ESTADO_PEDESTRE:
            gpio_put(LED_VERMELHO, 1);
            gpio_put(LED_VERDE, 0);
            printf("Sinal: Vermelho - Travessia de Pedestre\n");
            break;
    }
}

// Callback para desligar o buzzer após o pulso
bool buzzer_off_callback(repeating_timer_t *rt) {
    gpio_put(current_buzzer_gpio, 0);
    return false; // Não repete
}

void buzzer_pulse() {
    // Alterna entre os buzzers
    current_buzzer_gpio = usar_buzzer_a ? BUZZER_A : BUZZER_B;
    usar_buzzer_a = !usar_buzzer_a;

    // Liga o buzzer atual
    gpio_put(current_buzzer_gpio, 1);

    // Agenda para desligar o buzzer após 200 ms
    add_repeating_timer_ms(-200, buzzer_off_callback, NULL, &buzzer_timer);
}

bool contagem_callback(repeating_timer_t *rt) {
    if (contagem_regressiva > 0) {
        printf("Contagem Regressiva: %d\n", contagem_regressiva);
        buzzer_pulse();  // Emite pulso não bloqueante
        contagem_regressiva--;
        return true;  // Continua a contagem
    } else {
        aguardando_fim_contagem = false;
        add_repeating_timer_ms(-1, timer_callback, NULL, &timer); // Continua fluxo normal
        return false;
    }
}

bool timer_callback(repeating_timer_t *rt) {
    static bool esperando_pedestre = false;

    if (aguardando_fim_contagem) return false;

    if (botao_pressionado && !esperando_pedestre) {
        estado_atual = ESTADO_AMARELO;
        atualiza_semaforo(ESTADO_AMARELO);
        esperando_pedestre = true;
        add_repeating_timer_ms(-3000, timer_callback, NULL, &timer); // 3s amarelo
    } 
    else if (esperando_pedestre) {
        estado_atual = ESTADO_PEDESTRE;
        atualiza_semaforo(ESTADO_PEDESTRE);

        add_repeating_timer_ms(-5000, timer_callback, NULL, &timer); // 5s antes da contagem

        contagem_regressiva = 5;
        aguardando_fim_contagem = true;
        usar_buzzer_a = true;  // Reseta alternância
        add_repeating_timer_ms(-1000, contagem_callback, NULL, &contagem_timer);

        botao_pressionado = false;
        esperando_pedestre = false;
    } 
    else {
        switch (estado_atual) {
            case ESTADO_VERMELHO:
                estado_atual = ESTADO_VERDE;
                atualiza_semaforo(ESTADO_VERDE);
                add_repeating_timer_ms(-10000, timer_callback, NULL, &timer); // 10s verde
                break;
            case ESTADO_VERDE:
                estado_atual = ESTADO_AMARELO;
                atualiza_semaforo(ESTADO_AMARELO);
                add_repeating_timer_ms(-3000, timer_callback, NULL, &timer); // 3s amarelo
                break;
            case ESTADO_AMARELO:
            case ESTADO_PEDESTRE:
            default:
                estado_atual = ESTADO_VERMELHO;
                atualiza_semaforo(ESTADO_VERMELHO);
                add_repeating_timer_ms(-10000, timer_callback, NULL, &timer); // 10s vermelho
                break;
        }
    }
    return false;
}

void setup_gpio() {
    gpio_init(LED_VERMELHO);
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);

    gpio_init(LED_VERDE);
    gpio_set_dir(LED_VERDE, GPIO_OUT);

    gpio_init(BOTAO_PEDE);
    gpio_set_dir(BOTAO_PEDE, GPIO_IN);
    gpio_pull_up(BOTAO_PEDE);

    gpio_init(BUZZER_A);
    gpio_set_dir(BUZZER_A, GPIO_OUT);
    gpio_put(BUZZER_A, 0);

    gpio_init(BUZZER_B);
    gpio_set_dir(BUZZER_B, GPIO_OUT);
    gpio_put(BUZZER_B, 0);

    gpio_set_irq_enabled_with_callback(BOTAO_PEDE, GPIO_IRQ_EDGE_FALL, true, &botao_callback);
}

int main() {
    stdio_init_all();
    setup_gpio();

    atualiza_semaforo(ESTADO_VERMELHO);
    add_repeating_timer_ms(-10000, timer_callback, NULL, &timer); // 10s vermelho inicial

    while (true) {
        tight_loop_contents();
    }
}
