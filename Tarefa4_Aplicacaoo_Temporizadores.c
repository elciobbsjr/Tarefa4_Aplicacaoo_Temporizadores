// Inclusão das bibliotecas necessárias
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "inc/ssd1306.h"

// Definição dos pinos utilizados no projeto
#define LED_VERMELHO 13 // LED representando o sinal vermelho
#define LED_VERDE    11 // LED representando o sinal verde
#define BOTAO_A      5  // Botão de pedestre (Centro)
#define BOTAO_B      6  // Botão de pedestre (Bairro)
#define BUZZER_A     21 // Buzzer A para alerta sonoro
#define BUZZER_B     10 // Buzzer B alternativo para acessibilidade

// Pinos para comunicação I2C com o display OLED
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

// Definição dos tempos de controle do semáforo e travessia
#define TEMPO_VERMELHO     10000 // 10s no vermelho
#define TEMPO_VERDE        10000 // 10s no verde
#define TEMPO_AMARELO      3000  // 3s no amarelo
#define TEMPO_TRAVESSIA    5000  // 5s de travessia para pedestre
#define INTERVALO_CONTAGEM 1000  // 1s entre cada decremento da contagem
#define TEMPO_BUZZER       200   // Buzzer ativo por 200ms

// Enumeração dos estados possíveis do semáforo
typedef enum { 
    ESTADO_VERMELHO, 
    ESTADO_VERDE, 
    ESTADO_AMARELO, 
    ESTADO_PEDESTRE_A, 
    ESTADO_PEDESTRE_B 
} estado_t;

// Variáveis de controle de estado e lógica
volatile estado_t estado_atual = ESTADO_VERMELHO;
volatile bool pedido_A = false;
volatile bool pedido_B = false;
volatile int contagem_regressiva = 0;
volatile bool aguardando_fim_contagem = false;
volatile bool usar_buzzer_a = true;
volatile uint current_buzzer_gpio = BUZZER_A;

// Instâncias dos temporizadores
repeating_timer_t timer;
repeating_timer_t contagem_timer;
repeating_timer_t buzzer_timer;

// Prototipação das funções utilizadas
bool timer_callback(repeating_timer_t *rt);
bool contagem_callback(repeating_timer_t *rt);
void atualizar_display(const char* status, int contagem);

// Callback dos botões de pedestre com prioridade para Botão A (Centro)
void botao_callback(uint gpio, uint32_t events) {
    if (gpio == BOTAO_A) {
        pedido_A = true;
        pedido_B = false; // Se A foi pressionado, B é ignorado
        printf("Botao A (Centro) acionado\n");
        atualizar_display("Botao A\nCentro", 0); // Exibe mensagem no display OLED
    } else if (gpio == BOTAO_B && !pedido_A) {
        pedido_B = true;
        printf("Botao B (Bairro) acionado\n");
        atualizar_display("Botao B\nBairro", 0); // Exibe mensagem no display OLED
    }
}

// Callback para desligar o buzzer após o tempo configurado
bool buzzer_off_callback(repeating_timer_t *rt) {
    gpio_put(current_buzzer_gpio, 0); // Desliga o buzzer atual
    return false;
}

// Função para emitir pulso sonoro alternando entre dois buzzers
void buzzer_pulse() {
    current_buzzer_gpio = usar_buzzer_a ? BUZZER_A : BUZZER_B;
    usar_buzzer_a = !usar_buzzer_a;
    gpio_put(current_buzzer_gpio, 1); // Liga o buzzer
    add_repeating_timer_ms(-TEMPO_BUZZER, buzzer_off_callback, NULL, &buzzer_timer); // Desliga após 200ms
}

// Atualiza o display OLED com a mensagem de status e contagem regressiva, se houver
void atualizar_display(const char* status, int contagem) {
    struct render_area area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&area);
    uint8_t buffer[ssd1306_buffer_length];
    memset(buffer, 0, ssd1306_buffer_length); // Limpa o buffer antes de desenhar

    ssd1306_draw_string(buffer, 0, 0, (char*)status); // Exibe o status na primeira linha

    if (contagem > 0) {
        char countdown_str[16];
        sprintf(countdown_str, "Tempo: %d", contagem);
        ssd1306_draw_string(buffer, 0, 16, countdown_str); // Exibe a contagem na segunda linha
    }

    render_on_display(buffer, &area); // Renderiza no display OLED
    printf("%s\n", status); // Também imprime no Monitor Serial
}
// Atualiza os LEDs e o display conforme o estado atual do semaforo
void atualiza_semaforo(estado_t estado) {
    const char* display_status = "";

    switch (estado) {
        case ESTADO_VERMELHO:
            gpio_put(LED_VERMELHO, 1);
            gpio_put(LED_VERDE, 0);
            display_status = "Sinal:\nVermelho";
            break;
        case ESTADO_VERDE:
            gpio_put(LED_VERMELHO, 0);
            gpio_put(LED_VERDE, 1);
            display_status = "Sinal:\nVerde";
            break;
        case ESTADO_AMARELO:
            gpio_put(LED_VERMELHO, 1);
            gpio_put(LED_VERDE, 1); // Amarelo é a combinação de vermelho + verde
            display_status = "Sinal:\nAmarelo";
            break;
        case ESTADO_PEDESTRE_A:
            gpio_put(LED_VERMELHO, 1);
            gpio_put(LED_VERDE, 0);
            display_status = "Travessia\nCentro";
            break;
        case ESTADO_PEDESTRE_B:
            gpio_put(LED_VERMELHO, 1);
            gpio_put(LED_VERDE, 0);
            display_status = "Travessia\nBairro";
            break;
    }

    atualizar_display(display_status, contagem_regressiva);
}

// Callback para a contagem regressiva de travessia do pedestre
bool contagem_callback(repeating_timer_t *rt) {
    if (contagem_regressiva > 0) {
        printf("Contagem Regressiva: %d\n", contagem_regressiva);
        buzzer_pulse(); // Emite aviso sonoro a cada segundo

        atualizar_display(
            (estado_atual == ESTADO_PEDESTRE_A) ? "Travessia\nCentro" : "Travessia\nBairro", 
            contagem_regressiva
        );
        contagem_regressiva--;
        return true; // Continua a contagem
    } else {
        aguardando_fim_contagem = false;
        add_repeating_timer_ms(-1, timer_callback, NULL, &timer); // Retorna ao ciclo normal
        return false;
    }
}

// Callback principal que controla todo o fluxo do semaforo
bool timer_callback(repeating_timer_t *rt) {
    static bool em_travessia = false;

    if (aguardando_fim_contagem) return false;

    if (!em_travessia && (pedido_A || pedido_B)) {
        // Tratamento dos pedidos de travessia, priorizando o Botao A
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
        add_repeating_timer_ms(-TEMPO_AMARELO, timer_callback, NULL, &timer);
    } 
    else if (em_travessia) {
        atualiza_semaforo(estado_atual);
        add_repeating_timer_ms(-TEMPO_TRAVESSIA, timer_callback, NULL, &timer);

        contagem_regressiva = TEMPO_TRAVESSIA / 1000;
        aguardando_fim_contagem = true;
        usar_buzzer_a = true;
        add_repeating_timer_ms(-INTERVALO_CONTAGEM, contagem_callback, NULL, &contagem_timer);

        em_travessia = false;
    } 
    else {
        // Ciclo normal do semaforo
        switch (estado_atual) {
            case ESTADO_VERMELHO:
                estado_atual = ESTADO_VERDE;
                atualiza_semaforo(ESTADO_VERDE);
                add_repeating_timer_ms(-TEMPO_VERDE, timer_callback, NULL, &timer);
                break;
            case ESTADO_VERDE:
                estado_atual = ESTADO_AMARELO;
                atualiza_semaforo(ESTADO_AMARELO);
                add_repeating_timer_ms(-TEMPO_AMARELO, timer_callback, NULL, &timer);
                break;
            default:
                estado_atual = ESTADO_VERMELHO;
                atualiza_semaforo(ESTADO_VERMELHO);
                add_repeating_timer_ms(-TEMPO_VERMELHO, timer_callback, NULL, &timer);
                break;
        }
    }

    return false;
}

// Configuracao inicial do display OLED via I2C
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
    render_on_display(buffer, &area); // Limpa o display na inicializacao
}

// Configuracao inicial dos GPIOs e interrupcoes dos botoes
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

// Funcao principal onde tudo comeca
int main() {
    stdio_init_all(); // Inicializa a comunicacao serial
    setup_gpio();     // Configura GPIOs
    setup_display();  // Configura display

    atualiza_semaforo(ESTADO_VERMELHO); // Inicia com o semaforo em vermelho
    add_repeating_timer_ms(-TEMPO_VERMELHO, timer_callback, NULL, &timer); // Inicia o ciclo

    while (true) {
        tight_loop_contents(); // Mantem a CPU em espera; tudo é controlado via interrupcoes e timers
    }
}