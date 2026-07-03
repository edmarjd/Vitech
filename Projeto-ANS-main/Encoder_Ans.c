#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* 
 CONFIGURAÇÃO GLOBAL
 */

#define RANGE 8
#define CONFIG 6

#define EMPTY UINT8_MAX

#define MAX_SYMBOLS        1000000
#define BITSTREAM_CAPACITY 1000000

/* 
 FLUSH FIXO
 */

#if RANGE == 4
    #define FLUSH_BITS 2
    #define MAX_CONFIG 2
#elif RANGE == 6
    #define FLUSH_BITS 3
    #define MAX_CONFIG 4
#elif RANGE == 8
    #define FLUSH_BITS 3
    #define MAX_CONFIG 6
#elif RANGE == 9
    #define FLUSH_BITS 4
    #define MAX_CONFIG 7
#elif RANGE == 10
    #define FLUSH_BITS 4
    #define MAX_CONFIG 8
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

} BitstreamWriter;

typedef struct {

    uint32_t state;

} AnsState;

typedef struct {

    AnsState state;

} AnsContext;

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
    {0x0,0x4,0x2,0x6,0x1,0x5,0x3,0x7},
    {EMPTY,EMPTY,EMPTY,EMPTY,0,1,0,1},
    {0x0,0x2,0x1,0x3,0x0,0x2,0x1,0x3},
    {EMPTY,EMPTY,0,1,0,1,0,1},
    {0,1,0,1,0x0,0x2,0x1,0x3},
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
    {0x0,0x4,0x2,0x6,0x1,0x5,0x3,0x7},
    {EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,EMPTY,0,1},
    {EMPTY,EMPTY,EMPTY,EMPTY,0,1,0,1},
    {0x0,0x2,0x1,0x3,0x0,0x2,0x1,0x3},
    {EMPTY,EMPTY,0,1,0,1,0,1},
    {0,1,0,1,0x0,0x2,0x1,0x3},
    {0,1,0,1,0,1,0,1}
};

static const uint8_t b_nbits_range8[7][8] = {
    {3,3,3,3,3,3,3,3},
    {0,0,0,0,0,0,1,1},
    {0,0,0,0,1,1,1,1},
    {2,2,2,2,2,2,2,2},
    {0,0,1,1,1,1,1,1},
    {1,1,1,1,2,2,2,2},
    {1,1,1,1,1,1,1,1}
};

/* 
 TABELAS COMPILE-TIME
 */

#if RANGE == 8

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

    stream->buffer = global_bitstream_buffer;

    stream->byte_pos = 0;

    stream->bit_pos = 7;

    stream->total_bits_written = 0;

    stream->capacity = BITSTREAM_CAPACITY;
}

void bitstream_write_bits(BitstreamWriter *stream, uint32_t bits, int nbits ) {

    for (int i = nbits - 1; i >= 0; i--) {

        if (stream->byte_pos >= stream->capacity) {

            fprintf(stderr, "ERRO: bitstream cheio\n");

            return;
        }

        uint8_t bit = (bits >> i) & 1;

        if (bit) {

            stream->buffer[stream->byte_pos]|= (1 << stream->bit_pos);
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

    uint32_t final_state = context.state.state;

    uint32_t state_offset = final_state - RANGE;

    bitstream_write_bits(&bitstream,state_offset,FLUSH_BITS);

    DBG_PRINTF("\nOffset final: %u\n",state_offset);

    char output_filename[256];

    int i = 0;

    while (filename[i] != '\0' && filename[i] != '.' && i < 240 ) {

        output_filename[i] = filename[i];

        i++;
    }

    const char suffix[] = "_encoded.bin";

    int j = 0;

    while (suffix[j] != '\0') {

        output_filename[i++] = suffix[j++];
    }

    output_filename[i] = '\0';

    bitstream_save_binary( &bitstream, output_filename, (uint32_t)symbol_count );

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