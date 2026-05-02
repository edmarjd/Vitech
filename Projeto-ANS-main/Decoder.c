/*
 * Documentação Geral do Programa
 *
 * Este programa implementa um decodificador adaptativo baseado em table-based Asymmetric Numeral Systems (tANS)
 * para símbolos binários (0 e 1, referidos como A e B). Ele processa arquivos .txt codificados, decodifica-os usando tANS com adaptação periódica do modelo e gera saídas de diagnóstico,
 * incluindo os símbolos decodificados e verificações.
 *
 * Principais Conceitos:
 * - tANS: Técnica de codificação de entropia usando tabelas pré-computadas para transições de estado.
 * - Adaptação: A cada X símbolos, seleciona o melhor modelo baseado em probabilidades observadas.
 * - Range (L): Tamanho da dispersão (lido do arquivo codificado).
 * - Config: Configuração dentro do range, definindo probabilidade de A (0).
 * - Estado: Valor em [L, 2L-1], atualizado por símbolo.
 *
 * Bibliotecas: stdio, stdint, stdlib, string, math, bool, limits.
 *
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <limits.h>

#define EMPTY 255

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

/* Tabelas para Range 8 (L=8) */
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
    {EMPTY, EMPTY, 0, 1, 0, 1},
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
const int freq_a_range4[3] = {3, 1, 2};
const int freq_a_range6[5] = {5, 1, 4, 2, 3};
const int freq_a_range8[7] = {7, 1, 6, 2, 5, 3, 4};
const int freq_a_range9[8] = {8, 1, 7, 2, 6, 3, 5, 4};
const int freq_a_range10[9] = {9, 1, 8, 2, 7, 3, 6, 4, 5};

/* Estruturas */
typedef struct {
    uint32_t state;
    int current_config;
} AnsState;

typedef struct {
    const char *bit_string;
    int total_bits;
    int current_bit_pos;
    int total_bits_read;
} StringBitstreamReader;

typedef struct {
    int symbol;
    uint8_t nbits;
    uint8_t base_state;
} DecodeEntry;

/* Tabelas de Decodificação Globais */
DecodeEntry g_rlt_range4[3][4];
DecodeEntry g_rlt_range6[5][6];
DecodeEntry g_rlt_range8[7][8];
DecodeEntry g_rlt_range9[8][9];
DecodeEntry g_rlt_range10[9][10];

/* Globais */
int g_range = 0;
int g_adapt_interval = 0;
int g_total_symbols = 0;

/* Funções */

// Alterei a função ans_find_best_model para refletir o último símbolo

int ans_find_best_model_viciado(int ultimo_simbolo, int config_atual) {
    int r = g_range; // Usa a variável global do Decoder.c
    int nc;
    
    // Define o número de configurações baseado no Range atual
    if (r == 8) nc = 7;
    else if (r == 6) nc = 5;
    else if (r == 9) nc = 8;
    else if (r == 10) nc = 9;
    else if (r == 4) nc = 3;
    else return config_atual;

    // Se o último símbolo decodificado foi 0, aumenta a probabilidade de 0 (sobe na tabela de frequências de A)
    if (ultimo_simbolo == 0) {
        if (config_atual < nc - 1) return config_atual + 1; 
    } 
    // Se o último símbolo foi 1, aumenta a probabilidade de 1 (desce na tabela de frequências de A)
    else if (ultimo_simbolo == 1) {
        if (config_atual > 0) return config_atual - 1;
    }
    
    return config_atual;
}

void build_rlt_range4() {
    const int L = 4;
    const int NC = 3;
    for (int cfg = 0; cfg < NC; cfg++) {
        for (int i = 0; i < L; i++) g_rlt_range4[cfg][i].symbol = -1;
        for (int prev_idx = 0; prev_idx < L; prev_idx++) {
            uint8_t next_state = a_range4[cfg][prev_idx];
            int next_state_idx = next_state - L;
            if (next_state_idx < 0 || next_state_idx >= L) continue;
            if (g_rlt_range4[cfg][next_state_idx].symbol == -1) {
                g_rlt_range4[cfg][next_state_idx] = (DecodeEntry){
                    0, a_nbits_range4[cfg][prev_idx], L + prev_idx
                };
            }
        }
        for (int prev_idx = 0; prev_idx < L; prev_idx++) {
            uint8_t next_state = b_range4[cfg][prev_idx];
            int next_state_idx = next_state - L;
            if (next_state_idx < 0 || next_state_idx >= L) continue;
            if (g_rlt_range4[cfg][next_state_idx].symbol == -1) {
                g_rlt_range4[cfg][next_state_idx] = (DecodeEntry){
                    1, b_nbits_range4[cfg][prev_idx], L + prev_idx
                };
            }
        }
    }
}

void build_rlt_range6() {
    const int L = 6;
    const int NC = 5;
    for (int cfg = 0; cfg < NC; cfg++) {
        for (int i = 0; i < L; i++) g_rlt_range6[cfg][i].symbol = -1;
        for (int prev_idx = 0; prev_idx < L; prev_idx++) {
            uint8_t next_state = a_range6[cfg][prev_idx];
            int next_state_idx = next_state - L;
            if (next_state_idx < 0 || next_state_idx >= L) continue;
            if (g_rlt_range6[cfg][next_state_idx].symbol == -1) {
                g_rlt_range6[cfg][next_state_idx] = (DecodeEntry){
                    0, a_nbits_range6[cfg][prev_idx], L + prev_idx
                };
            }
        }
        for (int prev_idx = 0; prev_idx < L; prev_idx++) {
            uint8_t next_state = b_range6[cfg][prev_idx];
            int next_state_idx = next_state - L;
            if (next_state_idx < 0 || next_state_idx >= L) continue;
            if (g_rlt_range6[cfg][next_state_idx].symbol == -1) {
                g_rlt_range6[cfg][next_state_idx] = (DecodeEntry){
                    1, b_nbits_range6[cfg][prev_idx], L + prev_idx
                };
            }
        }
    }
}

void build_rlt_range8() {
    const int L = 8;
    const int NC = 7;
    for (int cfg = 0; cfg < NC; cfg++) {
        for (int i = 0; i < L; i++) g_rlt_range8[cfg][i].symbol = -1;
        for (int prev_idx = 0; prev_idx < L; prev_idx++) {
            uint8_t next_state = a_range8[cfg][prev_idx];
            int next_state_idx = next_state - L;
            if (next_state_idx < 0 || next_state_idx >= L) continue;
            if (g_rlt_range8[cfg][next_state_idx].symbol == -1) {
                g_rlt_range8[cfg][next_state_idx] = (DecodeEntry){
                    0, a_nbits_range8[cfg][prev_idx], L + prev_idx
                };
            }
        }
        for (int prev_idx = 0; prev_idx < L; prev_idx++) {
            uint8_t next_state = b_range8[cfg][prev_idx];
            int next_state_idx = next_state - L;
            if (next_state_idx < 0 || next_state_idx >= L) continue;
            if (g_rlt_range8[cfg][next_state_idx].symbol == -1) {
                g_rlt_range8[cfg][next_state_idx] = (DecodeEntry){
                    1, b_nbits_range8[cfg][prev_idx], L + prev_idx
                };
            }
        }
    }
}

void build_rlt_range9() {
    const int L = 9;
    const int NC = 8;
    for (int cfg = 0; cfg < NC; cfg++) {
        for (int i = 0; i < L; i++) g_rlt_range9[cfg][i].symbol = -1;
        for (int prev_idx = 0; prev_idx < L; prev_idx++) {
            uint8_t next_state = a_range9[cfg][prev_idx];
            int next_state_idx = next_state - L;
            if (next_state_idx < 0 || next_state_idx >= L) continue;
            if (g_rlt_range9[cfg][next_state_idx].symbol == -1) {
                g_rlt_range9[cfg][next_state_idx] = (DecodeEntry){
                    0, a_nbits_range9[cfg][prev_idx], L + prev_idx
                };
            }
        }
        for (int prev_idx = 0; prev_idx < L; prev_idx++) {
            uint8_t next_state = b_range9[cfg][prev_idx];
            int next_state_idx = next_state - L;
            if (next_state_idx < 0 || next_state_idx >= L) continue;
            if (g_rlt_range9[cfg][next_state_idx].symbol == -1) {
                g_rlt_range9[cfg][next_state_idx] = (DecodeEntry){
                    1, b_nbits_range9[cfg][prev_idx], L + prev_idx
                };
            }
        }
    }
}

void build_rlt_range10() {
    const int L = 10;
    const int NC = 9;
    for (int cfg = 0; cfg < NC; cfg++) {
        for (int i = 0; i < L; i++) g_rlt_range10[cfg][i].symbol = -1;
        for (int prev_idx = 0; prev_idx < L; prev_idx++) {
            uint8_t next_state = a_range10[cfg][prev_idx];
            int next_state_idx = next_state - L;
            if (next_state_idx < 0 || next_state_idx >= L) continue;
            if (g_rlt_range10[cfg][next_state_idx].symbol == -1) {
                g_rlt_range10[cfg][next_state_idx] = (DecodeEntry){
                    0, a_nbits_range10[cfg][prev_idx], L + prev_idx
                };
            }
        }
        for (int prev_idx = 0; prev_idx < L; prev_idx++) {
            uint8_t next_state = b_range10[cfg][prev_idx];
            int next_state_idx = next_state - L;
            if (next_state_idx < 0 || next_state_idx >= L) continue;
            if (g_rlt_range10[cfg][next_state_idx].symbol == -1) {
                g_rlt_range10[cfg][next_state_idx] = (DecodeEntry){
                    1, b_nbits_range10[cfg][prev_idx], L + prev_idx
                };
            }
        }
    }
}

void string_reader_init(StringBitstreamReader *reader, const char *bit_string) {
    reader->bit_string = bit_string;
    reader->total_bits = strlen(bit_string);
    reader->current_bit_pos = reader->total_bits - 1;
    reader->total_bits_read = 0;
}

uint32_t bitstream_read_bits_backward(StringBitstreamReader *reader, int nbits) {
    uint32_t result = 0;
    for (int i = 0; i < nbits; i++) {
        if (reader->current_bit_pos < 0) {
            fprintf(stderr, "ERRO: Leitura (backward) alem do inicio do bitstream!\n");
            return UINT32_MAX;
        } else {
            char bit_char = reader->bit_string[reader->current_bit_pos--];
            if (bit_char == '1') {
                result |= (1 << i);
            }
        }
        reader->total_bits_read++;
    }
    return result;
}

int ans_decode_symbol(AnsState *ans_state, StringBitstreamReader *reader) {
    int L = g_range;
    int config = ans_state->current_config;
    uint32_t state = ans_state->state;
    int state_idx = state - L;
    if (state_idx < 0 || state_idx >= L) {
        fprintf(stderr, "ERRO DECODE: Estado invalido state=%u, L=%d, idx=%d\n", state, L, state_idx);
        return -1;
    }
    const DecodeEntry *entry = NULL;
    if (L == 8) {
        entry = &g_rlt_range8[config][state_idx];
    } else if (L == 6) {
        entry = &g_rlt_range6[config][state_idx];
    } else if (L == 9) {
        entry = &g_rlt_range9[config][state_idx];
    } else if (L == 10) {
        entry = &g_rlt_range10[config][state_idx];
    } else if (L == 4) {
        entry = &g_rlt_range4[config][state_idx];
    } else {
        fprintf(stderr, "ERRO DECODE: Range %d invalido!\n", L);
        return -1;
    }
    if (entry->symbol == -1) {
        fprintf(stderr, "ERRO DECODE: RLT nao populada! state=%u, L=%d, idx=%d, cfg=%d\n", state, L, state_idx, config);
        return -1;
    }
    uint32_t r = 0;
    if (entry->nbits > 0) {
        r = bitstream_read_bits_backward(reader, entry->nbits);
        if (r == UINT32_MAX) return -1;
    }
    ans_state->state = entry->base_state + r;
    return entry->symbol;
}

char *read_dynamic_line(FILE *f) {
    size_t capacity = 1024;
    char *line = malloc(capacity);
    if (!line) return NULL;
    size_t length = 0;
    line[0] = '\0';
    while (1) {
        if (fgets(line + length, capacity - length, f) == NULL) {
            if (length == 0) {
                free(line);
                return NULL;
            }
            break;
        }
        length += strlen(line + length);
        if (length > 0 && line[length - 1] == '\n') {
            line[length - 1] = '\0';
            break;
        }
        if (length == capacity - 1) {
            capacity *= 2;
            char *new_line = realloc(line, capacity);
            if (!new_line) {
                free(line);
                return NULL;
            }
            line = new_line;
        }
    }
    return line;
}

char *parse_encoded_file(const char *filename, int *out_total_symbols, int *out_range, int *out_adapt_interval) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Erro ao abrir arquivo: %s\n", filename);
        return NULL;
    }
    char *bitstream_string = NULL;
    int in_header = 0;
    int in_bitstream = 0;
    *out_total_symbols = 0;
    *out_range = 0;
    *out_adapt_interval = 0;
    while (1) {
        char *line = read_dynamic_line(f);
        if (!line) break;
        if (strcmp(line, "HEADER") == 0) {
            in_header = 1;
            free(line);
            continue;
        } else if (strcmp(line, "END_HEADER") == 0) {
            in_header = 0;
            free(line);
            continue;
        } else if (in_header) {
            if (strstr(line, "TOTAL_SYMBOLS:") == line) {
                sscanf(line, "TOTAL_SYMBOLS:%d", out_total_symbols);
            } else if (strstr(line, "RANGE:") == line) {
                sscanf(line, "RANGE:%d", out_range);
            } else if (strstr(line, "ADAPTATION_INTERVAL:") == line) {
                sscanf(line, "ADAPTATION_INTERVAL:%d", out_adapt_interval);
            }
        }
        if (strcmp(line, "ADAPTATIONS") == 0) {
            in_header = 0;
            free(line);
            while (1) {
                line = read_dynamic_line(f);
                if (!line) break;
                if (strcmp(line, "END_ADAPTATIONS") == 0) break;
                free(line);
            }
            free(line);
            continue;
        }
        if (strcmp(line, "BITSTREAM") == 0) {
            in_bitstream = 1;
            free(line);
            bitstream_string = read_dynamic_line(f);
            continue;
        } else if (strcmp(line, "END_BITSTREAM") == 0) {
            in_bitstream = 0;
            free(line);
            continue;
        }
        free(line);
    }
    fclose(f);
    if (*out_range == 0) {
        fprintf(stderr, "Erro: RANGE nao encontrado no arquivo.\n");
        free(bitstream_string);
        return NULL;
    }
    if (*out_total_symbols == 0) {
        fprintf(stderr, "Erro: TOTAL_SYMBOLS nao encontrado no arquivo.\n");
        free(bitstream_string);
        return NULL;
    }
    if (bitstream_string == NULL || strlen(bitstream_string) == 0) {
        fprintf(stderr, "Erro: BITSTREAM vazio ou nao encontrado.\n");
        free(bitstream_string);
        return NULL;
    }
    return bitstream_string;
}

void decode_file(const char *input_encoded_file, const char *output_file) {
    printf("\n+=====================================================================+\n");
    printf("| Decodificando Arquivo (Modo Estático Viciado): %-20s |\n", input_encoded_file);
    printf("+=====================================================================+\n\n");

    char *bitstream_string = parse_encoded_file(input_encoded_file, &g_total_symbols, &g_range, &g_adapt_interval);
    if (!bitstream_string) {
        fprintf(stderr, "Falha ao processar arquivo codificado.\n");
        return;
    }

    int config_bits, flush_bits;
    // Configuração de bits e construção das tabelas RLT baseado no Range
    if (g_range == 8) { config_bits = 3; flush_bits = 3; build_rlt_range8(); }
    else if (g_range == 6) { config_bits = 3; flush_bits = 3; build_rlt_range6(); }
    else if (g_range == 9) { config_bits = 3; flush_bits = 4; build_rlt_range9(); }
    else if (g_range == 10) { config_bits = 4; flush_bits = 4; build_rlt_range10(); }
    else if (g_range == 4) { config_bits = 2; flush_bits = 2; build_rlt_range4(); }
    else {
        fprintf(stderr, "RANGE %d nao suportado!\n", g_range);
        free(bitstream_string);
        return;
    }

    int *decoded_symbols = malloc(g_total_symbols * sizeof(int));
    if (!decoded_symbols) {
        perror("malloc decoded_symbols");
        free(bitstream_string);
        return;
    }

    // Inicialização do Reader e do Estado
    StringBitstreamReader reader;
    string_reader_init(&reader, bitstream_string);
    AnsState context;

    // No modo estático viciado, precisamos ler o estado inicial (flush) e a config final
    printf("Lendo metadados de inicialização (Backward)...\n");
    
    // 1. O flush_bits representa o estado inicial da ANS para o último símbolo codificado
    uint32_t flush_val = bitstream_read_bits_backward(&reader, flush_bits);
    context.state = g_range + flush_val;

    // 2. Precisamos saber qual era a configuração de probabilidade no final do arquivo[cite: 1]
    uint32_t last_config = bitstream_read_bits_backward(&reader, config_bits);
    context.current_config = last_config;

    printf("Estado Inicial: %u | Config Final: %u\n", context.state, context.current_config);
    printf("Iniciando decodificação reativa...\n");

    int decoded_count = 0;
    while (decoded_count < g_total_symbols) {
        // Decodifica o símbolo usando a config atual[cite: 1]
        int symbol = ans_decode_symbol(&context, &reader);
        if (symbol == -1) break;

        decoded_symbols[decoded_count++] = symbol;

        // LÓGICA DE VÍCIO: Atualiza a configuração para o próximo símbolo (ordem backward)[cite: 1]
        // Se aparecer 1, aumenta a probabilidade do 1; se aparecer 0, aumenta a do 0.
        context.current_config = ans_find_best_model_viciado(symbol, context.current_config);
    }

    // Como tANS é backward, invertemos para obter a ordem original[cite: 1]
    printf("\nInvertendo ordem para restaurar original...\n");
    for (int i = 0; i < decoded_count / 2; i++) {
        int temp = decoded_symbols[i];
        decoded_symbols[i] = decoded_symbols[decoded_count - 1 - i];
        decoded_symbols[decoded_count - 1 - i] = temp;
    }

    // Salva o resultado[cite: 1]
    FILE *out_f = fopen(output_file, "w");
    if (out_f) {
        for (int i = 0; i < decoded_count; i++) fprintf(out_f, "%d\n", decoded_symbols[i]);
        fclose(out_f);
        printf("Arquivo salvo: %s (%d símbolos)\n", output_file, decoded_count);
    }

    free(bitstream_string);
    free(decoded_symbols);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: %s <arquivo_codificado.txt> <arquivo_saida.txt>\n", argv[0]);
        printf("Exemplo: %s input_encoded.txt output_decodificado.txt\n", argv[0]);
        return 1;
    }
    decode_file(argv[1], argv[2]);
    printf("\nDecodificacao concluida. Pressione Enter para sair...\n");
    getchar();
    return 0;
}
