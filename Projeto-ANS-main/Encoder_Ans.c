/*
 * Documentação Geral do Programa
 *
 * Este programa implementa um codificador adaptativo baseado em table-based Asymmetric Numeral Systems (tANS)
 * para símbolos binários (0 e 1, referidos como A e B). Ele processa arquivos .txt contendo sequências de
 * símbolos 0 e 1, codifica-os usando tANS com adaptação periódica do modelo e gera saídas de diagnóstico,
 * incluindo o bitstream gerado, estatísticas e métricas como entropia e comprimento médio de código.
 *
 * Principais Conceitos:
 * - tANS: Técnica de codificação de entropia usando tabelas pré-computadas para transições de estado.
 * - Adaptação: A cada X símbolos, seleciona o melhor modelo baseado em probabilidades observadas.
 * - Range (L): Tamanho da dispersão (FIXADO EM RANGE).
 * - Config: Configuração dentro do range, definindo probabilidade de A (0).
 * - Estado: Valor em [L, 2L-1], atualizado por símbolo.
 *
 * Bibliotecas: stdio, stdint, stdlib, string, math.
 *
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
/* Constantes Globais */
/* ============================================================================== */
/* Define o range (L) a ser usado. */
/* Valores válidos: 4, 6, 8, 9 ou 10. */
#define RANGE 8
/* ============================================================================== */
/* EMPTY: Valor sentinela para "sem bits a emitir" nas tabelas de bitstream. */
#define EMPTY 255
/* ADAPTATION_INTERVAL: Intervalo de símbolos entre adaptações do modelo (16 por padrão). */
#define ADAPTATION_INTERVAL 32
/* Tabelas para Range 9 (L=9) */
const uint8_t a_range9[8][9] = {
    {10, 11, 12, 13, 14, 15, 16, 9, 9},
    {9, 9, 9, 9, 9, 9, 9, 9, 9},
    {11, 12, 13, 14, 15, 9, 9, 10, 10},
    {9, 9, 9, 10, 10, 10, 10, 9, 9},
    {12, 13, 14, 9, 9, 10, 10, 11, 11},
    {10, 11, 11, 9, 9, 9, 9, 10, 10},
    {13, 9, 9, 10, 10, 11, 11, 12, 12},
    {9, 10, 10, 11, 11, 12, 12, 9, 9}
};
const uint8_t a_bitstream_range9[8][9] = {
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, 0, 1},
    {0x4, 0x2, 0x6, 0x1, 0x5, 0x3, 0x7, 0x0, 0x8},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, 0, 1, 0, 1},
    {0x2, 0x1, 0x3, 0x0, 0x2, 0x1, 0x3, 0x0, 0x4},
    {EMPTY, EMPTY, EMPTY, 0, 1, 0, 1, 0, 1},
    {1, 0, 1, 0x0, 0x2, 0x1, 0x3, 0x0, 0x2},
    {EMPTY, 0, 1, 0, 1, 0, 1, 0, 1},
    {1, 0, 1, 0, 1, 0, 1, 0x0, 0x2}
};
const uint8_t a_nbits_range9[8][9] = {
    {0, 0, 0, 0, 0, 0, 0, 1, 1},
    {3, 3, 3, 3, 3, 3, 3, 4, 4},
    {0, 0, 0, 0, 0, 1, 1, 1, 1},
    {2, 2, 2, 2, 2, 2, 2, 3, 3},
    {0, 0, 0, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 2, 2, 2, 2, 2, 2},
    {0, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 2, 2}
};
const uint8_t b_range9[8][9] = {
    {17, 17, 17, 17, 17, 17, 17, 17, 17},
    {11, 12, 13, 14, 15, 16, 17, 10, 10},
    {16, 16, 16, 17, 17, 17, 17, 16, 16},
    {13, 14, 15, 16, 17, 11, 11, 12, 12},
    {16, 17, 17, 15, 15, 15, 15, 16, 16},
    {15, 16, 17, 12, 12, 13, 13, 14, 14},
    {14, 15, 15, 16, 16, 17, 17, 14, 14},
    {17, 13, 13, 14, 14, 15, 15, 16, 16}
};
const uint8_t b_bitstream_range9[8][9] = {
    {0x4, 0x2, 0x6, 0x1, 0x5, 0x3, 0x7, 0x0, 0x8},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, 0, 1},
    {0x2, 0x1, 0x3, 0x0, 0x2, 0x1, 0x3, 0x0, 0x4},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, 0, 1, 0, 1},
    {1, 0, 1, 0x0, 0x2, 0x1, 0x3, 0x0, 0x2},
    {EMPTY, EMPTY, EMPTY, 0, 1, 0, 1, 0, 1},
    {1, 0, 1, 0, 1, 0, 1, 0x0, 0x2},
    {EMPTY, 0, 1, 0, 1, 0, 1, 0, 1}
};
const uint8_t b_nbits_range9[8][9] = {
    {3, 3, 3, 3, 3, 3, 3, 4, 4},
    {0, 0, 0, 0, 0, 0, 0, 1, 1},
    {2, 2, 2, 2, 2, 2, 2, 3, 3},
    {0, 0, 0, 0, 0, 1, 1, 1, 1},
    {1, 1, 1, 2, 2, 2, 2, 2, 2},
    {0, 0, 0, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 2, 2},
    {0, 1, 1, 1, 1, 1, 1, 1, 1}
};
/* Tabelas para Range 10 (L=10) */
const uint8_t a_range10[9][10] = {
    {11, 12, 13, 14, 15, 16, 17, 18, 10, 10},
    {10, 10, 10, 10, 10, 10, 10, 10, 10, 10},
    {12, 13, 14, 15, 16, 17, 10, 10, 11, 11},
    {10, 10, 11, 11, 11, 11, 10, 10, 10, 10},
    {13, 14, 15, 16, 10, 10, 11, 11, 12, 12},
    {12, 12, 10, 10, 10, 10, 11, 11, 11, 11},
    {14, 15, 10, 10, 11, 11, 12, 12, 13, 13},
    {11, 11, 12, 12, 13, 13, 10, 10, 10, 10},
    {10, 10, 11, 11, 12, 12, 13, 13, 14, 14}
};
const uint8_t a_bitstream_range10[9][10] = {
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, 0, 1},
    {0x2, 0x6, 0x1, 0x5, 0x3, 0x7, 0x0, 0x8, 0x4, 0xC},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, 0, 1, 0, 1},
    {0x1, 0x3, 0x0, 0x2, 0x1, 0x3, 0x0, 0x4, 0x2, 0x6},
    {EMPTY, EMPTY, EMPTY, EMPTY, 0, 1, 0, 1, 0, 1},
    {0, 1, 0x0, 0x2, 0x1, 0x3, 0x0, 0x2, 0x1, 0x3},
    {EMPTY, EMPTY, 0, 1, 0, 1, 0, 1, 0, 1},
    {0, 1, 0, 1, 0, 1, 0x0, 0x2, 0x1, 0x3},
    {0, 1, 0, 1, 0, 1, 0, 1, 0, 1}
};
const uint8_t a_nbits_range10[9][10] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
    {3, 3, 3, 3, 3, 3, 4, 4, 4, 4},
    {0, 0, 0, 0, 0, 0, 1, 1, 1, 1},
    {2, 2, 2, 2, 2, 2, 3, 3, 3, 3},
    {0, 0, 0, 0, 1, 1, 1, 1, 1, 1},
    {1, 1, 2, 2, 2, 2, 2, 2, 2, 2},
    {0, 0, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 2, 2, 2, 2},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
};
const uint8_t b_range10[9][10] = {
    {19, 19, 19, 19, 19, 19, 19, 19, 19, 19},
    {12, 13, 14, 15, 16, 17, 18, 19, 11, 11},
    {18, 18, 19, 19, 19, 19, 18, 18, 18, 18},
    {14, 15, 16, 17, 18, 19, 12, 12, 13, 13},
    {19, 19, 17, 17, 17, 17, 18, 18, 18, 18},
    {16, 17, 18, 19, 13, 13, 14, 14, 15, 15},
    {17, 17, 18, 18, 19, 19, 16, 16, 16, 16},
    {18, 19, 14, 14, 15, 15, 16, 16, 17, 17},
    {15, 15, 16, 16, 17, 17, 18, 18, 19, 19}
};
const uint8_t b_bitstream_range10[9][10] = {
    {0x2, 0x6, 0x1, 0x5, 0x3, 0x7, 0x0, 0x8, 0x4, 0xC},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, 0, 1},
    {0x1, 0x3, 0x0, 0x2, 0x1, 0x3, 0x0, 0x4, 0x2, 0x6},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, 0, 1, 0, 1},
    {0, 1, 0x0, 0x2, 0x1, 0x3, 0x0, 0x2, 0x1, 0x3},
    {EMPTY, EMPTY, EMPTY, EMPTY, 0, 1, 0, 1, 0, 1},
    {0, 1, 0, 1, 0, 1, 0x0, 0x2, 0x1, 0x3},
    {EMPTY, EMPTY, 0, 1, 0, 1, 0, 1, 0, 1},
    {0, 1, 0, 1, 0, 1, 0, 1, 0, 1}
};
const uint8_t b_nbits_range10[9][10] = {
    {3, 3, 3, 3, 3, 3, 4, 4, 4, 4},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
    {2, 2, 2, 2, 2, 2, 3, 3, 3, 3},
    {0, 0, 0, 0, 0, 0, 1, 1, 1, 1},
    {1, 1, 2, 2, 2, 2, 2, 2, 2, 2},
    {0, 0, 0, 0, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 2, 2, 2, 2},
    {0, 0, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
};
/* Tabelas para Range 8 */
const uint8_t a_range8[7][8] = {
    {9, 10, 11, 12, 13, 14, 8, 8},
    {8, 8, 8, 8, 8, 8, 8, 8},
    {10, 11, 12, 13, 8, 8, 9, 9},
    {8, 8, 8, 8, 9, 9, 9, 9},
    {11, 12, 8, 8, 9, 9, 10, 10},
    {9, 9, 10, 10, 8, 8, 8, 8},
    {8, 8, 9, 9, 10, 10, 11, 11}
};
const uint8_t a_bitstream_range8[7][8] = {
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, 0, 1},
    {0x0, 0x4, 0x2, 0x6, 0x1, 0x5, 0x3, 0x7},
    {EMPTY, EMPTY, EMPTY, EMPTY, 0, 1, 0, 1},
    {0x0, 0x2, 0x1, 0x3, 0x0, 0x2, 0x1, 0x3},
    {EMPTY, EMPTY, 0, 1, 0, 1, 0, 1},
    {0, 1, 0, 1, 0x0, 0x2, 0x1, 0x3},
    {0, 1, 0, 1, 0, 1, 0, 1}
};
const uint8_t a_nbits_range8[7][8] = {
    {0,0,0,0,0,0,1,1},
    {3,3,3,3,3,3,3,3},
    {0,0,0,0,1,1,1,1},
    {2,2,2,2,2,2,2,2},
    {0,0,1,1,1,1,1,1},
    {1,1,1,1,2,2,2,2},
    {1,1,1,1,1,1,1,1}
};
const uint8_t b_range8[7][8] = {
    {15, 15, 15, 15, 15, 15, 15, 15},
    {10, 11, 12, 13, 14, 15, 9, 9},
    {14, 14, 14, 14, 15, 15, 15, 15},
    {12, 13, 14, 15, 10, 10, 11, 11},
    {14, 14, 15, 15, 13, 13, 13, 13},
    {14, 15, 11, 11, 12, 12, 13, 13},
    {12, 12, 13, 13, 14, 14, 15, 15}
};
const uint8_t b_bitstream_range8[7][8] = {
    {0x0, 0x4, 0x2, 0x6, 0x1, 0x5, 0x3, 0x7},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, 0, 1},
    {EMPTY, EMPTY, EMPTY, EMPTY, 0, 1, 0, 1},
    {0x0, 0x2, 0x1, 0x3, 0x0, 0x2, 0x1, 0x3},
    {EMPTY, EMPTY, 0, 1, 0, 1, 0, 1},
    {0, 1, 0, 1, 0x0, 0x2, 0x1, 0x3},
    {0, 1, 0, 1, 0, 1, 0, 1}
};
const uint8_t b_nbits_range8[7][8] = {
    {3,3,3,3,3,3,3,3},
    {0,0,0,0,0,0,1,1},
    {0,0,0,0,1,1,1,1},
    {2,2,2,2,2,2,2,2},
    {0,0,1,1,1,1,1,1},
    {1,1,1,1,2,2,2,2},
    {1,1,1,1,1,1,1,1}
};
/* Tabelas para Range 6 (L=6) */
const uint8_t a_range6[5][6] = {
    {7, 8, 9, 10, 6, 6},
    {6, 6, 6, 6, 6, 6},
    {8, 9, 6, 6, 7, 7},
    {7, 7, 6, 6, 6, 6},
    {6, 6, 7, 7, 8, 8}
};
const uint8_t a_bitstream_range6[5][6] = {
    {EMPTY, EMPTY, EMPTY, EMPTY, 0, 1},
    {0x1, 0x3, 0x0, 0x4, 0x2, 0x6},
    {EMPTY, EMPTY, EMPTY, EMPTY, 0, 1},
    {0, 1, 0x0, 0x2, 0x1, 0x3},
    {0, 1, 0, 1, 0, 1}
};
const uint8_t a_nbits_range6[5][6] = {
    {0,0,0,0,1,1},
    {2,2,3,3,3,3},
    {0,0,1,1,1,1},
    {1,1,2,2,2,2},
    {1,1,1,1,1,1}
};
const uint8_t b_range6[5][6] = {
    {11, 11, 11, 11, 11, 11},
    {8, 9, 10, 11, 7, 7},
    {11, 11, 10, 10, 10, 10},
    {10, 11, 8, 8, 9, 9},
    {9, 9, 10, 10, 11, 11}
};
const uint8_t b_bitstream_range6[5][6] = {
    {0x1, 0x3, 0x0, 0x4, 0x2, 0x6},
    {0x1, 0x3, 0x0, 0x4, 0x2, 0x6},
    {0, 1, 0x0, 0x2, 0x1, 0x3},
    {EMPTY, EMPTY, 0, 1, 1, 1},
    {0, 1, 0, 1, 0, 1}
};
const uint8_t b_nbits_range6[5][6] = {
    {2,2,3,3,3,3},
    {2,2,3,3,3,3},
    {1,1,2,2,2,2},
    {0,0,1,1,1,1},
    {1,1,1,1,1,1}
};
/* Tabelas para Range 4 (L=4) */
const uint8_t a_range4[3][4] = {
    {5, 6, 4, 4},
    {4, 4, 4, 4},
    {4, 4, 5, 5}
};
const uint8_t a_bitstream_range4[3][4] = {
    {EMPTY, EMPTY, 0, 1},
    {0x0, 0x2, 0x1, 0x3},
    {0, 1, 0, 1}
};
const uint8_t a_nbits_range4[3][4] = {
    {0,0,1,1},
    {2,2,2,2},
    {1,1,1,1}
};
const uint8_t b_range4[3][4] = {
    {7, 7, 7, 7},
    {6, 7, 5, 5},
    {6, 6, 7, 7}
};
const uint8_t b_bitstream_range4[3][4] = {
    {0x0, 0x2, 0x1, 0x3},
    {EMPTY, EMPTY, 0, 1},
    {0, 1, 0, 1}
};
const uint8_t b_nbits_range4[3][4] = {
    {2,2,2,2},
    {0,0,1,1},
    {1,1,1,1}
};
/* Arrays de Frequências */
/* Usados para calcular probabilidades em ans_find_best_model. */
/* freq_a_range4: Frequências de A para configs de range 4 (probs: 3/4, 1/4, 2/4). */
const int freq_a_range4[3] = {3, 1, 2};
/* freq_a_range6: Para range 6. */
const int freq_a_range6[5] = {5, 1, 4, 2, 3};
/* freq_a_range8: Para range 8. */
const int freq_a_range8[7] = {7, 1, 6, 2, 5, 3, 4};
const int freq_a_range9[8] = {8, 1, 7, 2, 6, 3, 5, 4};
const int freq_a_range10[9] = {9, 1, 8, 2, 7, 3, 6, 4, 5};
/* Estruturas de Dados */
/* ModelChoice: Armazena escolha de modelo tANS. */
/* - range: O range (L: 4,6,8,9,10). */
/* - config: Configuração dentro do range (0 a num_configs-1). */
typedef struct { int range; int config; } ModelChoice;
/* BitstreamWriter: Gerencia o buffer de bits de saída. */
/* - buffer: Ponteiro para array de bytes alocado. */
/* - byte_pos: Posição atual do byte no buffer. */
/* - bit_pos: Posição do bit no byte atual (7 a 0, MSB-first). */
/* - total_bits_written: Contagem total de bits escritos. */
typedef struct { uint8_t *buffer; size_t byte_pos; int bit_pos; int total_bits_written; size_t capacity; } BitstreamWriter;
/* AnsState: Estado atual do tANS. */
/* - state: Valor do estado (em [L, 2L-1]). */
/* - current_range: Range atual (L). */
/* - current_config: Configuração atual. */
typedef struct { uint32_t state; int current_range; int current_config; } AnsState;
/* AnsContext: Contexto completo do codificador tANS. */
/* - state: O estado tANS. */
/* - adapt_count_A: Contagem de símbolos 0 (A) desde última adaptação. */
/* - adapt_count_B: Contagem de símbolos 1 (B) desde última adaptação. */
/* - symbols_since_adapt: Símbolos processados desde última adaptação. */
typedef struct {
    AnsState state;
    int adapt_count_A;
    int adapt_count_B;
    int symbols_since_adapt;
} AnsContext;
/* Protótipos de Funções */
int encoder_find_best_model_viciado(int simbolo_atual, int config_atual);
void bitstream_init(BitstreamWriter *stream, size_t size);
void bitstream_write_bits(BitstreamWriter *stream, uint32_t bits, int nbits);
void bitstream_print(BitstreamWriter* stream);
void bitstream_save_encoded_data(BitstreamWriter* stream, const char* output_filename,
                                int total_symbols);
void ans_encode_symbol(AnsContext *context, int symbol, BitstreamWriter *stream, char* stream_op_out);
ModelChoice ans_find_best_model(int count_a, int count_b);
void ans_set_model(AnsContext *context, int new_range, int new_config);
void process_file(const char *filename);
/*
 * Função: ans_find_best_model
 * Propósito: Seleciona o melhor modelo (range fixo + melhor config) baseado em contagens de A e B,
 * minimizando a entropia cruzada (custo esperado).
 * Parâmetros:
 * - count_a: Contagem de símbolos 0.
 * - count_b: Contagem de símbolos 1.
 * Retorno: Struct ModelChoice com o range fixo e a melhor config.
 */
ModelChoice ans_find_best_model(int count_a, int count_b) {
    const int r = RANGE;
    int nc;
    const int* freqs;
    ModelChoice best_model;
    int default_config;
    if (r == 8) {
        nc = 7;
        freqs = freq_a_range8;
        default_config = 6; // P(A) = 4/8
    } else if (r == 6) {
        nc = 5;
        freqs = freq_a_range6;
        default_config = 4; // P(A) = 3/6
    } else if (r == 9) {
        nc = 8;
        freqs = freq_a_range9;
        default_config = 7; // P(A) ≈ 4/9 (mais próximo de 0.5)
    } else if (r == 10) {
        nc = 9;
        freqs = freq_a_range10;
        default_config = 8; // P(A) = 5/10
    } else if (r == 4) {
        nc = 3;
        freqs = freq_a_range4;
        default_config = 2; // P(A) = 2/4
    } else {
        fprintf(stderr, "ERRO: RANGE invalido! (%d)\n", RANGE);
        return (ModelChoice){8, 6}; // Fallback para R8, C6
    }
    best_model = (ModelChoice){r, default_config};
    // Se não houver contagens (início), retorna o padrão para o range
    if (count_a + count_b == 0) {
        return best_model;
    }
    float prob_a = (float)count_a / (count_a + count_b);
    float min_cost = 1e9;
    best_model.range = r;
    // Itera apenas sobre as configurações do range
    for (int cfg = 0; cfg < nc; cfg++) {
        int freq_a = freqs[cfg];
        if (freq_a <= 0 || freq_a >= r) continue;
        float model_prob_a = (float)freq_a / (float)r;
        float model_prob_b = 1.0f - model_prob_a;
        float cost = -(prob_a * log2f(model_prob_a) + (1.0f - prob_a) * log2f(model_prob_b));
        if (cost < min_cost) {
            min_cost = cost;
            best_model.config = cfg;
        }
    }
    return best_model;
}
/*
 * Função: bitstream_init
 * Propósito: Inicializa o escritor de bitstream, alocando buffer e definindo posições iniciais.
 */
void bitstream_init(BitstreamWriter *stream, size_t size) {
    stream->buffer = (uint8_t *)calloc(size, 1);
    if (!stream->buffer) { perror("calloc"); exit(1); }
    stream->byte_pos = 0; stream->bit_pos = 7; stream->total_bits_written = 0;
    stream->capacity = size;
}
/*
 * Função: bitstream_write_bits
 * Propósito: Escreve nbits bits no bitstream, MSB-first.
 */
void bitstream_write_bits(BitstreamWriter *stream, uint32_t bits, int nbits) {
    for (int i = nbits - 1; i >= 0; i--) {
        uint8_t bit = (bits >> i) & 1;
        if (bit) stream->buffer[stream->byte_pos] |= (1 << stream->bit_pos);
        stream->total_bits_written++;
        if (--stream->bit_pos < 0) {
            stream->bit_pos = 7;
            stream->byte_pos++;
            if (stream->byte_pos >= stream->capacity) {
                size_t new_capacity = stream->capacity * 2;
                uint8_t *new_buffer = (uint8_t *)realloc(stream->buffer, new_capacity);
                if (!new_buffer) { perror("realloc"); exit(1); }
                memset(new_buffer + stream->capacity, 0, new_capacity - stream->capacity);
                stream->buffer = new_buffer;
                stream->capacity = new_capacity;
            }
        }
    }
}
/*
 * Função: bitstream_save_encoded_data
 * Propósito: Salva todos os dados necessários para decodificação em um arquivo.
 */
void bitstream_save_encoded_data(BitstreamWriter* stream, const char* output_filename,
                                int total_symbols) {
    FILE *f_out = fopen(output_filename, "w");
    if (f_out == NULL) {
        fprintf(stderr, "Erro ao tentar criar o arquivo de saida: %s\n", output_filename);
        perror("Detalhe do erro");
        return;
    }
    // 1. Escrever cabeçalho com metadados
    fprintf(f_out, "HEADER\n");
    fprintf(f_out, "TOTAL_SYMBOLS:%d\n", total_symbols);
    fprintf(f_out, "RANGE:%d\n", RANGE);
    fprintf(f_out, "ADAPTATION_INTERVAL:%d\n", ADAPTATION_INTERVAL);
    fprintf(f_out, "END_HEADER\n");
    // 3. Escrever o bitstream
    fprintf(f_out, "BITSTREAM\n");
    int total_bits = stream->total_bits_written;
    int bits_printed_total = 0;
    for (size_t b = 0; b <= stream->byte_pos && bits_printed_total < total_bits; b++) {
        int bits_in_this_byte = (bits_printed_total + 8 <= total_bits) ? 8 : total_bits - bits_printed_total;
        for (int i = 7; i >= 8 - bits_in_this_byte; i--) {
            fprintf(f_out, "%c", ((stream->buffer[b] >> i) & 1) ? '1' : '0');
        }
        bits_printed_total += bits_in_this_byte;
    }
    fprintf(f_out, "\nEND_BITSTREAM\n");
    fclose(f_out);
    printf("| SUCESSO: Dados de codificação salvos em '%-46s' | \n", output_filename);
}
/*
 * Função: ans_encode_symbol
 * Propósito: Codifica um símbolo (0 ou 1) usando tANS, emitindo bits se necessário e atualizando estado.
 */
void ans_encode_symbol(AnsContext *context, int symbol, BitstreamWriter *stream, char* hex_output_log) {
    AnsState *ans_state = &context->state;
    int config = ans_state->current_config;
    int range = ans_state->current_range;
    uint32_t state = ans_state->state;
    const int L = range;
    int state_idx = state - L;
    if (state_idx < 0 || state_idx >= range) {
        fprintf(stderr, "ERRO: tANS estado invalido! state=%u, L=%d, idx=%d\n", state, L, state_idx);
        return;
    }
    uint8_t new_state, bits_out, nbits;
    if (symbol == 0) {
        if (range == 8) {
            new_state = a_range8[config][state_idx];
            bits_out = a_bitstream_range8[config][state_idx];
            nbits = a_nbits_range8[config][state_idx];
        } else if (range == 6) {
            new_state = a_range6[config][state_idx];
            bits_out = a_bitstream_range6[config][state_idx];
            nbits = a_nbits_range6[config][state_idx];
        } else if (range == 9) {
            new_state = a_range9[config][state_idx];
            bits_out = a_bitstream_range9[config][state_idx];
            nbits = a_nbits_range9[config][state_idx];
        } else if (range == 10) {
            new_state = a_range10[config][state_idx];
            bits_out = a_bitstream_range10[config][state_idx];
            nbits = a_nbits_range10[config][state_idx];
        } else {
            new_state = a_range4[config][state_idx];
            bits_out = a_bitstream_range4[config][state_idx];
            nbits = a_nbits_range4[config][state_idx];
        }
    } else {
        if (range == 8) {
            new_state = b_range8[config][state_idx];
            bits_out = b_bitstream_range8[config][state_idx];
            nbits = b_nbits_range8[config][state_idx];
        } else if (range == 6) {
            new_state = b_range6[config][state_idx];
            bits_out = b_bitstream_range6[config][state_idx];
            nbits = b_nbits_range6[config][state_idx];
        } else if (range == 9) {
            new_state = b_range9[config][state_idx];
            bits_out = b_bitstream_range9[config][state_idx];
            nbits = b_nbits_range9[config][state_idx];
        } else if (range == 10) {
            new_state = b_range10[config][state_idx];
            bits_out = b_bitstream_range10[config][state_idx];
            nbits = b_nbits_range10[config][state_idx];
        } else {
            new_state = b_range4[config][state_idx];
            bits_out = b_bitstream_range4[config][state_idx];
            nbits = b_nbits_range4[config][state_idx];
        }
    }
    if (nbits > 0 && bits_out != EMPTY) {
        bitstream_write_bits(stream, bits_out, nbits);
        char temp_log[64];
        snprintf(temp_log, 64, "%db (val %Xh) ", nbits, bits_out);
        strncat(hex_output_log, temp_log, 256 - strlen(hex_output_log) - 1);
    } else {
        strncat(hex_output_log, "0b ", 256 - strlen(hex_output_log) - 1);
    }
    ans_state->state = (uint32_t)new_state;
}
/*
 * Função: ans_set_model
 * Propósito: Define ou muda o modelo tANS.
 */
void ans_set_model(AnsContext *context, int new_range, int new_config) {
    int old_range = context->state.current_range;
    if (old_range > 0 && new_range != old_range) {
        uint32_t old_state = context->state.state;
        uint32_t new_state = ((uint64_t)old_state * (uint64_t)new_range) / (uint64_t)old_range;
        if (new_state < new_range) new_state = new_range;
        if (new_state >= (new_range * 2)) new_state = (new_range * 2) - 1;
        context->state.state = new_state;
    } else if (old_range <= 0) {
        context->state.state = new_range;
    }
    context->state.current_range = new_range;
    context->state.current_config = new_config;
}
/*
 * Função: bitstream_print
 * Propósito: Imprime o bitstream como string de bits (ex: '1010 1101') em formato de tabela.
 */
void bitstream_print(BitstreamWriter* stream) {
    int total_bits = stream->total_bits_written;
    if (total_bits <= 0) {
        printf("+-------------------------+-----------------------------------------------------------+\n");
        printf("| BitStream (0 bits) | '' |\n");
        return;
    }
    size_t approx_len = (size_t)total_bits + (total_bits / 8) + 32;
    char *bit_string = (char*)malloc(approx_len);
    if (!bit_string) {
        fprintf(stderr, "Erro: falha ao alocar memória em bitstream_print.\n");
        return;
    }
    bit_string[0] = '\0';
    size_t pos = 0;
    int bits_printed_total = 0;
    for (size_t b = 0; b <= stream->byte_pos && bits_printed_total < total_bits; b++) {
        int bits_in_this_byte = (bits_printed_total + 8 <= total_bits) ? 8 : total_bits - bits_printed_total;
        for (int i = 7; i >= 8 - bits_in_this_byte; i--) {
            bit_string[pos++] = ((stream->buffer[b] >> i) & 1) ? '1' : '0';
        }
        bits_printed_total += bits_in_this_byte;
        if(bits_printed_total < total_bits && bits_in_this_byte == 8) {
            bit_string[pos++] = ' ';
        }
    }
    bit_string[pos] = '\0';
    printf("+-------------------------+-----------------------------------------------------------+\n");
    printf("| BitStream (%-3d bits) | '%-55s' |\n", stream->total_bits_written, bit_string);
    free(bit_string);
}
/*
 * Função: process_file
 * Propósito: Processa um arquivo: Lê símbolos, codifica com ANS adaptativo e imprime resultados.
 * Parâmetros:
 * - filename: Nome do arquivo .txt.
 * Retorno: Nenhum. Libera memórias alocadas.
 */

void process_file(const char *filename) {
    printf("\n+=====================================================================+\n");
    printf("| Processando Arquivo (Modo Estático Viciado): %-22s |\n", filename);
    printf("+=====================================================================+\n\n");
    
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Erro ao tentar abrir o arquivo: %s\n", filename);
        perror("Detalhe do erro");
        return;
    }

    // --- LEITURA DO ARQUIVO (Mantida a lógica original) ---
    int symbol_count = 0;
    int symbol_capacity = 1024;
    int *symbols_buffer = malloc(symbol_capacity * sizeof(int));
    if (symbols_buffer == NULL) { perror("Erro malloc"); fclose(file); return; }
    int symbol;
    while (fscanf(file, "%d", &symbol) == 1) {
        if (symbol_count >= symbol_capacity) {
            symbol_capacity *= 2;
            symbols_buffer = realloc(symbols_buffer, symbol_capacity * sizeof(int));
        }
        if (symbol == 0 || symbol == 1) symbols_buffer[symbol_count++] = symbol;
    }
    fclose(file);
    if (symbol_count == 0) { free(symbols_buffer); return; }

    printf("========= CODIFICANDO DADOS (VÍCIO IMEDIATO) [RANGE: %d] =========\n", RANGE);
    
    AnsContext context;
    memset(&context, 0, sizeof(AnsContext));
    BitstreamWriter bitstream;
    bitstream_init(&bitstream, 1024);

    // 1. DEFINIÇÃO DO MODELO INICIAL
    // Começamos com a configuração padrão (50/50 ou o mais próximo disso).
    ModelChoice current_model = ans_find_best_model(0, 0); 
    ans_set_model(&context, current_model.range, current_model.config);
    
    // Define quantos bits usar para salvar a configuração baseado no RANGE.
    int bits_for_config = (RANGE == 10) ? 4 : (RANGE == 4) ? 2 : 3;

    // 2. ESCREVE A CONFIGURAÇÃO INICIAL NO BITSTREAM
    // Isso é necessário para o Decoder saber qual modelo foi usado no primeiro bit (Forward).
    bitstream_write_bits(&bitstream, (uint32_t)current_model.config, bits_for_config);

    printf("%-6s %-10s %s\n", "Input", "State", "Stream op (bits)");
    printf("--------------------------------------\n");

    clock_t start = clock();
    int total_count_A = 0, total_count_B = 0;

    // 3. LOOP DE CODIFICAÇÃO COM VÍCIO SÍMBOLO A SÍMBOLO
    for (int i = 0; i < symbol_count; i++) {
        char hex_output_log[256] = "";
        int current_symbol = symbols_buffer[i];

        // Codifica o símbolo usando a configuração vigente[cite: 1].
        ans_encode_symbol(&context, current_symbol, &bitstream, hex_output_log);
        
        printf("%-6d %-10u %s\n", current_symbol, context.state.state, hex_output_log);

        if (current_symbol == 0) total_count_A++; else total_count_B++;

        // LÓGICA DE VÍCIO: Atualiza a configuração para o PRÓXIMO símbolo imediatamente[cite: 1].
        // Esta função deve ser a 'ans_find_best_model_viciado' que criamos anteriormente.
        context.state.current_config = encoder_find_best_model_viciado(current_symbol, context.state.current_config);
    }

    // 4. FINALIZAÇÃO (FLUSH E CONFIG FINAL)
    printf("\n--- tANS FLUSH FINAL ---\n");
    uint32_t final_state = context.state.state;
    uint32_t state_offset = final_state - RANGE;
    int flush_bits = (RANGE == 9 || RANGE == 10) ? 4 : (RANGE == 4) ? 2 : 3;

    // Escreve os bits restantes do estado final[cite: 1].
    bitstream_write_bits(&bitstream, state_offset, flush_bits);
    printf("Offset final: %u (%d bits)\n", state_offset, flush_bits);

    // ESCREVE A ÚLTIMA CONFIGURAÇÃO NO FIM DO ARQUIVO
    // Como o Decoder lê de trás para frente, ele lerá esta config primeiro[cite: 1].
    bitstream_write_bits(&bitstream, (uint32_t)context.state.current_config, bits_for_config);
    printf("Config final salva: %d (%d bits)\n", context.state.current_config, bits_for_config);

    clock_t end = clock();
    
    // --- RELATÓRIOS E SALVAMENTO (Mantido o padrão) ---
    double execution_time = (double)(end - start) / CLOCKS_PER_SEC;
    bitstream_print(&bitstream);
    
    char output_filename[FILENAME_MAX];
    strncpy(output_filename, filename, FILENAME_MAX - 1);
    char *dot = strrchr(output_filename, '.');
    if (dot) *dot = '\0';
    strncat(output_filename, "_encoded.txt", FILENAME_MAX - strlen(output_filename) - 1);
    
    bitstream_save_encoded_data(&bitstream, output_filename, symbol_count);
    
    free(bitstream.buffer);
    free(symbols_buffer);
}

// Função para o Encoder "viciar" na probabilidade do símbolo atual
int encoder_find_best_model_viciado(int simbolo_atual, int config_atual) {
    const int r = RANGE;
    int nc;
    
    if (r == 8) nc = 7;
    else if (r == 6) nc = 5;
    else if (r == 9) nc = 8;
    else if (r == 10) nc = 9;
    else if (r == 4) nc = 3;
    else return config_atual;

    // Se o símbolo codificado foi 0, favorece o 0 (sobe a config de A)
    if (simbolo_atual == 0) {
        if (config_atual < nc - 1) return config_atual + 1;
    } 
    // Se o símbolo foi 1, favorece o 1 (desce a config de A)
    else if (simbolo_atual == 1) {
        if (config_atual > 0) return config_atual - 1;
    }
    
    return config_atual;
}

/*
 * Função Principal: main
 * Propósito: Ponto de entrada;
 * Retorno: 0 (sucesso) ou 1 (nenhum arquivo encontrado).
 */
int main(int argc, char *argv[]) {
    printf("Iniciando processamento...\n");
    if (argc < 2) {
        fprintf(stderr, "Erro: Nenhum arquivo de entrada foi especificado.\n");
        fprintf(stderr, "Uso: %s <arquivo1.txt> [arquivo2.txt] ...\n", argv[0]);
        printf("\nPressione Enter para sair...\n");
        getchar();
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        process_file(argv[i]);
    }
    printf("\nProcessamento de todos os arquivos concluido.\n");
    printf("Pressione Enter para sair...\n");
    getchar();
    return 0;
}
