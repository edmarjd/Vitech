#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* 
 CONFIGURAÇÃO GLOBAL
 */

#ifndef RANGE
    #define RANGE 8
#endif

#ifndef CONFIG
    #define CONFIG 6
#endif

#define EMPTY UINT8_MAX

#define MAX_SYMBOLS        1000000
#define BITSTREAM_CAPACITY 1000000

/* 
 FLUSH FIXO
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
    /* Configs INVALIDOS: 0 (fa=5,fb=1) e 1 (fa=1,fb=5) — nbits não uniforme por next-state */
    #if CONFIG == 0 || CONFIG == 1
        #error "RANGE=6: CONFIG 0 e 1 geram tabelas ambíguas (multi-nbits no mesmo next-state). Use CONFIG 2,3 ou 4."
    #endif
    /* Configs válidos: 2 (fa=4,fb=2), 3 (fa=2,fb=4), 4 (fa=3,fb=3) */
#elif RANGE == 8
    #define FLUSH_BITS 3
    #define MAX_CONFIG 6
    /* Configs INVALIDOS: 2 (fa=6,fb=2) e 4 (fa=5,fb=3) — nb=0 ambíguo */
    #if CONFIG == 2 || CONFIG == 4
        #error "RANGE=8: CONFIG 2 e 4 geram tabelas ambíguas (nb=0 com multiplos prev-states). Use CONFIG 0,1,3,5 ou 6."
    #endif
    /* Configs válidos: 0 (fa=7,fb=1), 1 (fa=1,fb=7), 3 (fa=2,fb=6), 5 (fa=3,fb=5), 6 (fa=4,fb=4) */
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
 BUFFERS ESTÁTICOS
 */

static int symbols_buffer[MAX_SYMBOLS];

static uint8_t global_bitstream_buffer[BITSTREAM_CAPACITY];

/*
 ESTRUTURAS
 */

typedef struct {

    uint8_t *buffer;

    size_t byte_pos;

    int bit_pos;

    int total_bits_written;

    size_t capacity;

    int overflow;  /* BUG10: flag de overflow do buffer */

} BitstreamWriter;

typedef struct {

    uint32_t state;

} AnsState;

typedef struct {

    AnsState state;

} AnsContext;

/*
 TABELAS RANGE 2
 */

static const uint8_t a_range2[1][2] = {
    {2,2}
};

static const uint8_t a_bitstream_range2[1][2] = {
    {0,1}
};

static const uint8_t a_nbits_range2[1][2] = {
    {1,1}
};

static const uint8_t b_range2[1][2] = {
    {3,3}
};

static const uint8_t b_bitstream_range2[1][2] = {
    {0,1}
};

static const uint8_t b_nbits_range2[1][2] = {
    {1,1}
};

/*
 TABELAS RANGE 4
 */

static const uint8_t a_range4[3][4] = {
    {5,6,4,4},
    {4,4,4,4},
    {4,4,5,5}
};

static const uint8_t a_bitstream_range4[3][4] = {
    {EMPTY,EMPTY,0,1},
    {0,2,1,3},
    {0,1,0,1}
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

static const uint8_t b_bitstream_range4[3][4] = {
    {0,2,1,3},
    {EMPTY,EMPTY,0,1},
    {0,1,0,1}
};

static const uint8_t b_nbits_range4[3][4] = {
    {2,2,2,2},
    {0,0,1,1},
    {1,1,1,1}
};

/*
 TABELAS RANGE 6
 */

static const uint8_t a_range6[5][6] = {
    {7,8,9,10,6,6},
    {6,6,6,6,6,6},
    {8,9,6,6,7,7},
    {7,7,6,6,6,6},
    {6,6,7,7,8,8}
};

static const uint8_t a_bitstream_range6[5][6] = {
    {EMPTY,EMPTY,EMPTY,EMPTY,0,1},
    {1,3,0,4,2,6},
    {EMPTY,EMPTY,0,1,0,1},
    {0,1,0,2,1,3},
    {0,1,0,1,0,1}
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

static const uint8_t b_bitstream_range6[5][6] = {
    {1,3,0,4,2,6},
    {EMPTY,EMPTY,EMPTY,EMPTY,0,1},
    {0,1,0,2,1,3},
    {EMPTY,EMPTY,0,1,0,1},
    {0,1,0,1,0,1}
};

static const uint8_t b_nbits_range6[5][6] = {
    {2,2,3,3,3,3},
    {0,0,0,0,1,1},
    {1,1,2,2,2,2},
    {0,0,1,1,1,1},
    {1,1,1,1,1,1}
};

/*
 TABELAS RANGE 8
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

static const uint8_t a_bitstream_range8[7][8] = {
    {EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,0,1},
    {0,4,2,6,1,5,3,7},
    {EMPTY,EMPTY,EMPTY,EMPTY,0,1,0,1},
    {0,2,1,3,0,2,1,3},
    {EMPTY,EMPTY,0,1,0,1,0,1},
    {0,1,0,1,0,2,1,3},
    {0,1,0,1,0,1,0,1}
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

static const uint8_t b_bitstream_range8[7][8] = {
    {0,4,2,6,1,5,3,7},
    {EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,0,1},
    {0,2,1,3,0,2,1,3},
    {EMPTY,EMPTY,EMPTY,EMPTY,0,1,0,1},
    {0,1,0,1,0,2,1,3},
    {EMPTY,EMPTY,0,1,0,1,0,1},
    {0,1,0,1,0,1,0,1}
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

/* 
 TABELAS COMPILE-TIME
 */

#if RANGE == 2
    #define A_TABLE  a_range2[CONFIG]
    #define A_BITS   a_bitstream_range2[CONFIG]
    #define A_NBITS  a_nbits_range2[CONFIG]
    #define B_TABLE  b_range2[CONFIG]
    #define B_BITS   b_bitstream_range2[CONFIG]
    #define B_NBITS  b_nbits_range2[CONFIG]
#elif RANGE == 4
    #define A_TABLE  a_range4[CONFIG]
    #define A_BITS   a_bitstream_range4[CONFIG]
    #define A_NBITS  a_nbits_range4[CONFIG]
    #define B_TABLE  b_range4[CONFIG]
    #define B_BITS   b_bitstream_range4[CONFIG]
    #define B_NBITS  b_nbits_range4[CONFIG]
#elif RANGE == 6
    #define A_TABLE  a_range6[CONFIG]
    #define A_BITS   a_bitstream_range6[CONFIG]
    #define A_NBITS  a_nbits_range6[CONFIG]
    #define B_TABLE  b_range6[CONFIG]
    #define B_BITS   b_bitstream_range6[CONFIG]
    #define B_NBITS  b_nbits_range6[CONFIG]
#elif RANGE == 8
    #define A_TABLE  a_range8[CONFIG]
    #define A_BITS   a_bitstream_range8[CONFIG]
    #define A_NBITS  a_nbits_range8[CONFIG]
    #define B_TABLE  b_range8[CONFIG]
    #define B_BITS   b_bitstream_range8[CONFIG]
    #define B_NBITS  b_nbits_range8[CONFIG]
#else
    #error IMPLEMENTAR RANGE
#endif

/* 
 BITSTREAM
 */

void bitstream_init(BitstreamWriter *stream) {

    memset(global_bitstream_buffer, 0, BITSTREAM_CAPACITY);

    stream->buffer   = global_bitstream_buffer;
    stream->byte_pos = 0;
    stream->bit_pos  = 7;
    stream->total_bits_written = 0;
    stream->capacity = BITSTREAM_CAPACITY;
    stream->overflow = 0;  /* BUG10 */
}

void bitstream_write_bits(BitstreamWriter *stream, uint32_t bits, int nbits) {

    for (int i = nbits - 1; i >= 0; i--) {

        if (stream->byte_pos >= stream->capacity) {

            /* BUG10: marca overflow e aborta sem corromper estado */
            if (!stream->overflow) {
                fprintf(stderr, "ERRO: bitstream cheio — arquivo de saída pode estar corrompido.\n");
                stream->overflow = 1;
            }
            return;
        }

        uint8_t bit = (bits >> i) & 1;

        if (bit) {

            stream->buffer[stream->byte_pos] |= (1 << stream->bit_pos);
        }

        stream->total_bits_written++;

        stream->bit_pos--;

        if (stream->bit_pos < 0) {

            stream->bit_pos = 7;

            stream->byte_pos++;
        }
    }
}

/* 
 tANS
 */

void ans_init(AnsContext *context) {

    context->state.state = RANGE;
}

void ans_encode_symbol(AnsContext *context, int symbol, BitstreamWriter *stream ) {

    uint32_t state = context->state.state;

    int state_idx = state - RANGE;

    uint8_t new_state;
    uint8_t bits_out;
    uint8_t nbits;

    if (symbol == 0) {

        new_state = A_TABLE[state_idx];
        bits_out  = A_BITS[state_idx];
        nbits     = A_NBITS[state_idx];

    } else {

        new_state = B_TABLE[state_idx];
        bits_out  = B_BITS[state_idx];
        nbits     = B_NBITS[state_idx];
    }

    if (nbits > 0 && bits_out != EMPTY) {

        bitstream_write_bits(stream, bits_out, nbits);

        DBG_PRINTF("%db (0x%X)", nbits, bits_out);

    } else {

        DBG_PRINTF("0b");
    }

    context->state.state = new_state;
}

/* 
 SAVE BINÁRIO
 */

void bitstream_save_binary( BitstreamWriter *stream, const char *filename, uint32_t symbol_count) {

    FILE *f = fopen(filename, "wb");

    if (!f) {

        perror("fopen");

        return;
    }

    /* Grava o número de símbolos codificados como cabeçalho de 4 bytes */
    fwrite(&symbol_count, sizeof(uint32_t), 1, f);

    size_t total_bytes = (stream->total_bits_written + 7) / 8;

    fwrite(stream->buffer, 1, total_bytes, f);

    fclose(f);
}

/* 
 PROCESS FILE
 */

void process_file(const char *filename) {

    FILE *file = fopen(filename, "r");

    if (!file) {

        perror("fopen");

        return;
    }

    int symbol_count = 0;

    int symbol;

    while (fscanf(file, "%d", &symbol) == 1) {

        if (symbol_count >= MAX_SYMBOLS) {

            fprintf(stderr, "ERRO: muitos simbolos\n");

            fclose(file);

            return;
        }

        if (symbol == 0 || symbol == 1) {

            symbols_buffer[symbol_count++] = symbol;

        } else {

            /* BUG7: símbolo não-binário — avisa em vez de descartar silenciosamente */
            fprintf(stderr, "AVISO: símbolo inválido '%d' ignorado (esperado 0 ou 1).\n", symbol);
        }
    }

    fclose(file);

    AnsContext context;

    BitstreamWriter bitstream;

    ans_init(&context);

    bitstream_init(&bitstream);

    DBG_PRINTF("\n%-8s %-10s Bits\n", "Input", "Estado");

    DBG_PRINTF("----------------------------------\n");

    for (int i = 0; i < symbol_count; i++) {

        DBG_PRINTF("%-8d %-10u ",symbols_buffer[i],context.state.state);

        ans_encode_symbol(&context,symbols_buffer[i],&bitstream);

        DBG_PRINTF("\n");
    }

    if (bitstream.overflow) {
        fprintf(stderr, "ERRO: overflow do bitstream — codificacao abortada.\n");
        return;
    }

    uint32_t final_state = context.state.state;
    uint32_t state_offset = final_state - RANGE;

    bitstream_write_bits(&bitstream, state_offset, FLUSH_BITS);

    /* BUG2: stop bit — garante que o ultimo bit escrito e sempre 1.
     * O decoder faz padding-removal stripando zeros ate encontrar este 1. */
    bitstream_write_bits(&bitstream, 1, 1);

    DBG_PRINTF("\nOffset final: %u\n", state_offset);

    /* BUG6: usa strrchr para encontrar o ultimo ponto, evitando truncar
     * caminhos como /dir.v2/arquivo.txt → /dir_encoded.bin (errado). */
    char output_filename[512];
    const char *slash = strrchr(filename, '/');
    const char *dot   = strrchr(filename, '.');
    int base_len;
    if (dot && (!slash || dot > slash)) {
        base_len = (int)(dot - filename);
    } else {
        base_len = (int)strlen(filename);
    }
    if (base_len > 490) base_len = 490;
    strncpy(output_filename, filename, (size_t)base_len);
    strcpy(output_filename + base_len, "_encoded.bin");

    bitstream_save_binary(&bitstream, output_filename, (uint32_t)symbol_count);

    printf("Arquivo salvo: %s (%d simbolos)\n", output_filename, symbol_count);
}

/* 
 MAIN
 */

int main(int argc, char *argv[]) {

    printf("tANS Static Encoder\n");

    if (argc < 2) {

        fprintf(stderr,"Uso: %s <arquivo.txt>\n", argv[0]);

        return 1;
    }

    for (int i = 1; i < argc; i++) {

        process_file(argv[i]);
    }

    printf("\nProcessamento concluido.\n");

    return 0;
}