#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include <stdio.h>

#define LED_VERMELHO 13
#define LED_VERDE    11
#define BOTAO_PEDE   5

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

// Prototypes das funções de callback
bool timer_callback(repeating_timer_t *rt);
bool contagem_callback(repeating_timer_t *rt);


repeating_timer_t timer;
repeating_timer_t contagem_timer;

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

// Callback da contagem regressiva (executado a cada 1 segundo)
bool contagem_callback(repeating_timer_t *rt) {
    if (contagem_regressiva > 0) {
        printf("Contagem Regressiva: %d\n", contagem_regressiva);
        contagem_regressiva--;
        return true;  // Continua a contagem
    } else {
        aguardando_fim_contagem = false;  // Contagem terminou
        // Agora podemos avançar o estado do semáforo após a contagem regressiva
        add_repeating_timer_ms(-1, timer_callback, NULL, &timer); // Chama o timer_callback imediatamente
        return false; // Encerra a contagem
    }
}

// Callback principal do semáforo
bool timer_callback(repeating_timer_t *rt) {
    static bool esperando_pedestre = false;

    if (aguardando_fim_contagem) {
        // Ainda aguardando o final da contagem, não faz nada agora.
        return false;
    }

    if (botao_pressionado && !esperando_pedestre) {
        estado_atual = ESTADO_AMARELO;
        atualiza_semaforo(ESTADO_AMARELO);
        esperando_pedestre = true;
        add_repeating_timer_ms(-3000, timer_callback, NULL, &timer); // 3s de amarelo antes da travessia
    } 
    else if (esperando_pedestre) {
        estado_atual = ESTADO_PEDESTRE;
        atualiza_semaforo(ESTADO_PEDESTRE);

        // Aguarda 5 segundos e inicia contagem regressiva
        add_repeating_timer_ms(-5000, timer_callback, NULL, &timer);

        contagem_regressiva = 5;
        aguardando_fim_contagem = true;
        add_repeating_timer_ms(-1000, contagem_callback, NULL, &contagem_timer);

        botao_pressionado = false;
        esperando_pedestre = false;
    } 
    else {
        // Fluxo normal do semáforo
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

    gpio_set_irq_enabled_with_callback(BOTAO_PEDE, GPIO_IRQ_EDGE_FALL, true, &botao_callback);
}

int main() {
    stdio_init_all();
    setup_gpio();

    atualiza_semaforo(ESTADO_VERMELHO);
    add_repeating_timer_ms(-10000, timer_callback, NULL, &timer); // 10s inicial de vermelho

    while (true) {
        tight_loop_contents(); // Aguarda eventos
    }
}