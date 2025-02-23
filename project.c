#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"

// Definições de pinos
#define BTN_SEGURO 5       // Botão "Estou Seguro"
#define BTN_EMERGENCIA 6   // Botão "Não Estou Seguro"
#define LED_VERDE 11       // LED para status seguro
#define LED_VERMELHO 13    // LED para emergência
#define LED_AZUL 12        // LED para estado de espera
#define BUZZER_A 21        // Buzzer principal para alertas

// Definições do OLED
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

// Configuração dos tempos
#define TEMPO_LIMITE 30000 // 30 segundos antes da contagem regressiva
#define CONTAGEM_REGRESSIVA 10000 // 10 segundos de contagem regressiva

// Configurações do buzzer
#define FREQ_BASE 500      // Frequência base para o buzzer
#define FREQ_MAX 2000      // Frequência máxima para o buzzer
#define FREQ_EMERGENCIA 1000 // Frequência para alerta de emergência

// Variáveis globais para o OLED
struct render_area main_area;
uint8_t display_buffer[ssd1306_buffer_length];

// Função para configurar o buzzer com uma frequência específica
void configurar_buzzer(int frequencia) {
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_A);
    pwm_set_wrap(slice_num, 125000000 / frequencia);
    pwm_set_clkdiv(slice_num, 100);
}

// Função para tocar o buzzer
void tocar_buzzer(bool ligado) {
    pwm_set_gpio_level(BUZZER_A, ligado ? 32768 : 0);
}

// Função para atualizar o display com uma mensagem
void atualizar_display(const char* linha1, const char* linha2) {
    memset(display_buffer, 0, ssd1306_buffer_length);
    
    if (linha1) {
        ssd1306_draw_string(display_buffer, 0, 8, linha1);
    }
    if (linha2) {
        ssd1306_draw_string(display_buffer, 0, 24, linha2);
    }
    
    render_on_display(display_buffer, &main_area);
}

// Função para emitir um alerta de emergência
void emitir_alerta() {
    gpio_put(LED_AZUL, 0);     // Desliga LED azul
    gpio_put(LED_VERMELHO, 1); // Liga LED vermelho
    
    atualizar_display("  EMERGENCIA!", "Alerta Ativado");
    
    // Padrão de alerta de emergência
    for (int i = 0; i < 5; i++) {
        configurar_buzzer(FREQ_EMERGENCIA);
        tocar_buzzer(true);
        sleep_ms(500);
        tocar_buzzer(false);
        sleep_ms(200);
    }
    
    printf("🚨 EMERGÊNCIA ATIVADA!\n");
    sleep_ms(1000);
    gpio_put(LED_VERMELHO, 0); // Desliga LED vermelho
    gpio_put(LED_AZUL, 1);     // Volta para estado de espera
}

// Função para marcar status seguro
void marcar_seguro() {
    gpio_put(LED_AZUL, 0);    // Desliga LED azul
    gpio_put(LED_VERDE, 1);   // Liga LED verde
    
    atualizar_display("Status: SEGURO", "Prox. ver: 30min");
    
    printf("✅ Próxima verificação em 30 minutos.\n");
    sleep_ms(2000);
    gpio_put(LED_VERDE, 0);   // Desliga LED verde
    gpio_put(LED_AZUL, 1);    // Volta para estado de espera
}

int main() {
    stdio_init_all();

    // Configuração dos GPIOs
    gpio_init(BTN_SEGURO);
    gpio_set_dir(BTN_SEGURO, GPIO_IN);
    gpio_pull_up(BTN_SEGURO);

    gpio_init(BTN_EMERGENCIA);
    gpio_set_dir(BTN_EMERGENCIA, GPIO_IN);
    gpio_pull_up(BTN_EMERGENCIA);

    gpio_init(LED_VERDE);
    gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_put(LED_VERDE, 0);

    gpio_init(LED_VERMELHO);
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);
    gpio_put(LED_VERMELHO, 0);

    gpio_init(LED_AZUL);
    gpio_set_dir(LED_AZUL, GPIO_OUT);
    gpio_put(LED_AZUL, 1);  // Inicia com LED azul aceso

    // Inicialização do I2C e OLED
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    ssd1306_init();
    
    // Configuração da área de renderização
    main_area.start_column = 0;
    main_area.end_column = ssd1306_width - 1;
    main_area.start_page = 0;
    main_area.end_page = ssd1306_n_pages - 1;
    calculate_render_area_buffer_length(&main_area);

    // Limpa o display e mostra mensagem inicial
    memset(display_buffer, 0, ssd1306_buffer_length);
    atualizar_display("Sistema Iniciado", "Aguardando...");

    // Configuração do PWM para o buzzer
    gpio_set_function(BUZZER_A, GPIO_FUNC_PWM);
    pwm_set_wrap(pwm_gpio_to_slice_num(BUZZER_A), 62500);
    pwm_set_enabled(pwm_gpio_to_slice_num(BUZZER_A), true);

    while (1) {
        printf("Aguardando ação...\n");
        atualizar_display("Aguardando acao", "Pressione botao");
        
        int tempo_passado = 0;

        while (tempo_passado < TEMPO_LIMITE) {
            if (gpio_get(BTN_SEGURO) == 0) {
                marcar_seguro();
                tempo_passado = 0;  // Reinicia o contador
                continue;
            }

            if (gpio_get(BTN_EMERGENCIA) == 0) {
                emitir_alerta();
                tempo_passado = 0;  // Reinicia o contador
                continue;
            }

            sleep_ms(1000);
            tempo_passado += 1000;
        }

        if (tempo_passado >= TEMPO_LIMITE) {
            gpio_put(LED_AZUL, 0);     // Desliga LED azul
            gpio_put(LED_VERMELHO, 1); // Liga LED vermelho
            
            // Contagem regressiva com frequência crescente
            for (int t = 10; t > 0; t--) {
                char msg[32];
                sprintf(msg, "Tempo: %d seg", t);
                atualizar_display("ATENCAO!", msg);
                
                printf("⏳ Tempo restante: %d segundos\n", t);
                
                // Calcula frequência crescente baseada no tempo restante
                int freq = FREQ_BASE + ((FREQ_MAX - FREQ_BASE) * (10 - t) / 10);
                configurar_buzzer(freq);
                tocar_buzzer(true);
                sleep_ms(500);
                tocar_buzzer(false);
                sleep_ms(500);

                if (gpio_get(BTN_SEGURO) == 0) {
                    gpio_put(LED_VERMELHO, 0); // Desliga LED vermelho
                    marcar_seguro();
                    tempo_passado = 0;  // Reinicia o contador
                    break;
                }
            }

            // Se não pressionar nada durante a contagem regressiva
            if (tempo_passado != 0) {
                emitir_alerta();
                tempo_passado = 0;  // Reinicia o contador
            }
        }
    }
}