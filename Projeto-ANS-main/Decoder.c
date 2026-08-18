/*
 tANS STATIC DECODER
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifndef RANGE
    #define RANGE 8
#endif

#ifndef CONFIG
    #define CONFIG 6
#endif

/* Buffer estático para símbolos decodificados */
#define MAX_DECODED_SYMBOLS 1000000

#define EMPTY UINT8_MAX

#define BITSTREAM_CAPACITY 1000000

/*
 RANGE
 */

#if RANGE == 2
    #define FLUSH_BITS 1
    #define MAX_CONFIG 0
    /* Configs válidos: 0 (fa=1,fb=1) */
#elif RANGE == 4
    #define FLUSH_BITS 2
    #define MAX_CONFIG 2
    /* Configs válidos: 0 (fa=3,fb=1), 1 (fa=1,fb=3), 2 (fa=2,fb=2) */
#elif RANGE == 6
    #define FLUSH_BITS 3
    #define MAX_CONFIG 4
    /* Configs INVALIDOS: 0 (fa=5,fb=1) e 1 (fa=1,fb=5) — nbits não uniforme */
    #if CONFIG == 0 || CONFIG == 1
        #error "RANGE=6: CONFIG 0 e 1 geram tabelas ambíguas. Use CONFIG 2,3 ou 4."
    #endif
#elif RANGE == 8
    #define FLUSH_BITS 3
    #define MAX_CONFIG 6
    /* Configs INVALIDOS: 2 (fa=6,fb=2) e 4 (fa=5,fb=3) — nb=0 ambíguo */
    #if CONFIG == 2 || CONFIG == 4
        #error "RANGE=8: CONFIG 2 e 4 geram tabelas ambíguas. Use CONFIG 0,1,3,5 ou 6."
    #endif
#else
    #error RANGE invalido
#endif

#if CONFIG < 0 || CONFIG > MAX_CONFIG
    #error CONFIG invalida
#endif

/*
 DEBUG
 */

#define DEBUG 1

#if DEBUG
    #define DBG_PRINTF(...) printf(__VA_ARGS__)
#else
    #define DBG_PRINTF(...)
#endif

/*
 TABELAS RANGE 2 (Sincronizadas com Encoder)
 */

static const uint8_t a_range2[1][2] = {
    {2,2}
};

static const uint8_t a_nbits_range2[1][2] = {
    {1,1}
};

static const uint8_t b_range2[1][2] = {
    {3,3}
};

static const uint8_t b_nbits_range2[1][2] = {
    {1,1}
};

/*
 TABELAS RANGE 4 (Sincronizadas com Encoder)
 */

static const uint8_t a_range4[3][4] = {
    {5,6,4,4},
    {4,4,4,4},
    {4,4,5,5}
};

static const uint8_t a_nbits_range4[3][4] = {
    {0,0,1,1},
    {2,2,2,2},
    {1,1,1,1}
};

static const uint8_t b_range4[3][4] = {
    {7,7,7,7},
    {6,7,5,5},
    {6,6,7,7}
};

static const uint8_t b_nbits_range4[3][4] = {
    {2,2,2,2},
    {0,0,1,1},
    {1,1,1,1}
};

/*
 TABELAS RANGE 6 (Sincronizadas com Encoder)
 */

static const uint8_t a_range6[5][6] = {
    {7,8,9,10,6,6},
    {6,6,6,6,6,6},
    {8,9,6,6,7,7},
    {7,7,6,6,6,6},
    {6,6,7,7,8,8}
};

static const uint8_t a_nbits_range6[5][6] = {
    {0,0,0,0,1,1},
    {2,2,3,3,3,3},
    {0,0,1,1,1,1},
    {1,1,2,2,2,2},
    {1,1,1,1,1,1}
};

static const uint8_t b_range6[5][6] = {
    {11,11,11,11,11,11},
    {8,9,10,11,7,7},
    {11,11,10,10,10,10},
    {10,11,8,8,9,9},
    {9,9,10,10,11,11}
};

static const uint8_t b_nbits_range6[5][6] = {
    {2,2,3,3,3,3},
    {0,0,0,0,1,1},
    {1,1,2,2,2,2},
    {0,0,1,1,1,1},
    {1,1,1,1,1,1}
};

/*
 TABELAS RANGE 8 (Sincronizadas com Encoder)
 */

static const uint8_t a_range8[7][8] = {
    {9,10,11,12,13,14,8,8},
    {8,8,8,8,8,8,8,8},
    {10,11,12,13,8,8,9,9},
    {8,8,8,8,9,9,9,9},
    {11,12,8,8,9,9,10,10},
    {9,9,10,10,8,8,8,8},
    {8,8,9,9,10,10,11,11}
};

static const uint8_t a_nbits_range8[7][8] = {
    {0,0,0,0,0,0,1,1},
    {3,3,3,3,3,3,3,3},
    {0,0,0,0,1,1,1,1},
    {2,2,2,2,2,2,2,2},
    {0,0,1,1,1,1,1,1},
    {1,1,1,1,2,2,2,2},
    {1,1,1,1,1,1,1,1}
};

static const uint8_t b_range8[7][8] = {
    {15,15,15,15,15,15,15,15},
    {10,11,12,13,14,15,9,9},
    {14,14,14,14,15,15,15,15},
    {12,13,14,15,10,10,11,11},
    {14,14,15,15,13,13,13,13},
    {14,15,11,11,12,12,13,13},
    {12,12,13,13,14,14,15,15}
};

static const uint8_t b_nbits_range8[7][8] = {
    {3,3,3,3,3,3,3,3},
    {0,0,0,0,0,0,1,1},
    {2,2,2,2,2,2,2,2},
    {0,0,0,0,1,1,1,1},
    {1,1,1,1,2,2,2,2},
    {0,0,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1}
};

#if RANGE == 2
    #define A_TABLE  a_range2[CONFIG]
    #define A_NBITS  a_nbits_range2[CONFIG]
    #define B_TABLE  b_range2[CONFIG]
    #define B_NBITS  b_nbits_range2[CONFIG]
#elif RANGE == 4
    #define A_TABLE  a_range4[CONFIG]
    #define A_NBITS  a_nbits_range4[CONFIG]
    #define B_TABLE  b_range4[CONFIG]
    #define B_NBITS  b_nbits_range4[CONFIG]
#elif RANGE == 6
    #define A_TABLE  a_range6[CONFIG]
    #define A_NBITS  a_nbits_range6[CONFIG]
    #define B_TABLE  b_range6[CONFIG]
    #define B_NBITS  b_nbits_range6[CONFIG]
#elif RANGE == 8
    #define A_TABLE  a_range8[CONFIG]
    #define A_NBITS  a_nbits_range8[CONFIG]
    #define B_TABLE  b_range8[CONFIG]
    #define B_NBITS  b_nbits_range8[CONFIG]
#else
    #error RANGE invalido
#endif

/*
 ESTRUTURAS
 */

typedef struct {
    uint32_t state;
} AnsState;

typedef struct {
    int symbol;
    uint8_t nbits;
    uint8_t base_state;
} DecodeEntry;

typedef struct {
    uint8_t *buffer;
    int total_bits;
    int current_bit;
} BitstreamReader;

/*
 GLOBAIS
 */

static DecodeEntry g_rlt[RANGE];
static uint8_t bitstream_buffer[BITSTREAM_CAPACITY];
static int decoded_symbols[MAX_DECODED_SYMBOLS];

/*
 BUILD RLT
 */

void build_rlt() {
    for (int i = 0; i < RANGE; i++) {
        g_rlt[i].symbol = -1;
    }

    // Símbolo 0
    for (int prev_idx = 0; prev_idx < RANGE; prev_idx++) {
        uint8_t next_state = A_TABLE[prev_idx];
        int next_idx = next_state - RANGE;
        
        // Não sobrescrever se já estiver definido (preserva o base_state mínimo)
        if (g_rlt[next_idx].symbol == -1) {
            g_rlt[next_idx].symbol = 0;
            g_rlt[next_idx].nbits = A_NBITS[prev_idx];
            g_rlt[next_idx].base_state = RANGE + prev_idx;
        }
    }

    // Símbolo 1
    for (int prev_idx = 0; prev_idx < RANGE; prev_idx++) {
        uint8_t next_state = B_TABLE[prev_idx];
        int next_idx = next_state - RANGE;
        if (g_rlt[next_idx].symbol == -1) {
            g_rlt[next_idx].symbol = 1;
            g_rlt[next_idx].nbits = B_NBITS[prev_idx];
            g_rlt[next_idx].base_state = RANGE + prev_idx;
        }
    }

    // Bug fix: valida que todos os slots da RLT foram preenchidos
    for (int i = 0; i < RANGE; i++) {
        if (g_rlt[i].symbol == -1) {
            fprintf(stderr, "AVISO: RLT[%d] nao inicializada (CONFIG=%d). Tabelas inconsistentes.\n", i, CONFIG);
        }
    }
}

/*
 BITSTREAM
 */

void bitstream_reader_init(BitstreamReader *reader, uint8_t *buffer, int total_bits) {
    reader->buffer = buffer;
    reader->total_bits = total_bits;
    reader->current_bit = total_bits - 1;
}

// LSB-first: bit 0 é o primeiro lido do final (útil para valores normais)
uint32_t bitstream_read_bits_lsb(BitstreamReader *reader, int nbits) {
    uint32_t result = 0;
    for (int i = 0; i < nbits; i++) {
        if (reader->current_bit < 0) {
            return 0;
        }
        int bit_index = reader->current_bit;
        uint8_t bit = (reader->buffer[bit_index / 8] >> (7 - (bit_index % 8))) & 1;
        if (bit) result |= (1U << i);
        reader->current_bit--;
    }
    return result;
}

// MSB-first: desloca e adiciona (útil para compensar bit-reversal das tabelas)
uint32_t bitstream_read_bits_msb(BitstreamReader *reader, int nbits) {
    uint32_t result = 0;
    for (int i = 0; i < nbits; i++) {
        if (reader->current_bit < 0) {
            return 0;
        }
        int bit_index = reader->current_bit;
        uint8_t bit = (reader->buffer[bit_index / 8] >> (7 - (bit_index % 8))) & 1;
        result = (result << 1) | bit;
        reader->current_bit--;
    }
    return result;
}

/*
 DECODE
 */

int ans_decode_symbol(AnsState *state, BitstreamReader *reader) {
    int idx = state->state - RANGE;

    if (idx < 0 || idx >= RANGE) {
        fprintf(stderr, "Estado invalido: %u\n", state->state);
        return -1;
    }

    DecodeEntry *entry = &g_rlt[idx];

    uint32_t r = 0;
    if (entry->nbits > 0) {
        // Usa MSB para símbolos para desverter os bits da tabela do encoder
        r = bitstream_read_bits_msb(reader, entry->nbits);
    }

    state->state = entry->base_state + r;

    return entry->symbol;
}

/*
 PROCESS FILE
 */

void decode_file(const char *input_file, const char *output_file) {
    FILE *f = fopen(input_file, "rb");
    if (!f) {
        perror("fopen");
        return;
    }

    /* Lê cabeçalho: número de símbolos codificados (uint32_t, 4 bytes) */
    uint32_t num_symbols = 0;
    if (fread(&num_symbols, sizeof(uint32_t), 1, f) != 1) {
        fprintf(stderr, "ERRO: não foi possível ler o cabeçalho de num_symbols.\n");
        fclose(f);
        return;
    }

    if (num_symbols == 0 || num_symbols > MAX_DECODED_SYMBOLS) {
        fprintf(stderr, "ERRO: num_symbols inválido: %u\n", num_symbols);
        fclose(f);
        return;
    }

    /* Lê o restante (bitstream) depois do cabeçalho */
    size_t bytes_read = fread(bitstream_buffer, 1, BITSTREAM_CAPACITY, f);
    fclose(f);

    printf("Símbolos a decodificar: %u\n", num_symbols);

    /* BUG3: verifica leitura vazia */
    if (bytes_read == 0) {
        fprintf(stderr, "ERRO: arquivo comprimido vazio ou corrompido.\n");
        return;
    }

    int total_bits = (int)bytes_read * 8;

    /* Padding removal: remove zeros finais de alinhamento.
     * O encoder escreve um stop bit '1' após os flush bits,
     * portanto o loop para exatamente nesse '1'. */
    while (total_bits > FLUSH_BITS + 1) {
        int bit_index = total_bits - 1;
        if ((bitstream_buffer[bit_index / 8] >> (7 - (bit_index % 8))) & 1) {
            break;
        }
        total_bits--;
    }

    BitstreamReader reader;
    bitstream_reader_init(&reader, bitstream_buffer, total_bits);

    /* BUG2: o bit encontrado pelo padding removal é o stop bit '1'.
     * Pulamos ele antes de ler os flush bits. */
    reader.current_bit--;

    build_rlt();

    AnsState context;

    uint32_t offset = bitstream_read_bits_lsb(&reader, FLUSH_BITS);
    context.state = RANGE + offset;

    DBG_PRINTF("Offset extraido: %u | Estado inicial: %u\n", offset, context.state);

    int decode_ok = 1;
    for (int i = (int)num_symbols - 1; i >= 0; i--) {
        decoded_symbols[i] = ans_decode_symbol(&context, &reader);
        if (decoded_symbols[i] == -1) {
            fprintf(stderr, "ERRO: simbolo invalido na posicao %d — decodificacao abortada.\n", i);
            decode_ok = 0;
            break;  // Bug fix: para imediatamente, evita continuar com estado corrompido
        }
    }

    if (!decode_ok) {
        fprintf(stderr, "ERRO: arquivo nao pôde ser decodificado corretamente.\n");
        return;
    }

    FILE *out = fopen(output_file, "w");
    /* BUG9: exibe mensagem de erro em vez de retornar silenciosamente */
    if (!out) {
        perror("fopen saida");
        return;
    }
    for (uint32_t i = 0; i < num_symbols; i++) {
        fprintf(out, "%d", decoded_symbols[i]);
        if (i < num_symbols - 1) {
            fprintf(out, " ");
        }
    }
    fclose(out);

    printf("Arquivo decodificado: %s (%u simbolos)\n", output_file, num_symbols);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        return 1;
    }
    decode_file(argv[1], argv[2]);
    return 0;
}
