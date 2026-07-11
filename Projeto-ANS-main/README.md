# tANS Static Encoder/Decoder

Implementação de um codificador e decodificador **tANS (table-based Asymmetric Numeral Systems)** estático para símbolos binários (`0` e `1`) utilizando tabelas pré-computadas.

O projeto utiliza:

- `RANGE = 8`
- `CONFIG = 6`
- Codificação baseada em tabelas estáticas
- Escrita manual de bitstream
- Flush fixo do estado final
- Decodificação reversa utilizando RLT (Reverse Lookup Table)

---

# O que é tANS?

O **tANS** é uma técnica moderna de compressão entrópica utilizada em algoritmos de alta performance, como:

- Zstandard (Zstd)
- JPEG XL
- Sistemas de compressão de jogos
- Compressão em tempo real

O algoritmo substitui técnicas tradicionais como Huffman por um modelo baseado em:

- estados internos
- tabelas de transição
- emissão variável de bits

Diferente de algoritmos clássicos, o tANS mantém um estado interno que evolui conforme os símbolos são processados.

---

# Funcionamento do Encoder

O encoder:

1. Inicia com um estado inicial (`RANGE`)
2. Para cada símbolo:
   - consulta tabelas estáticas
   - gera bits
   - atualiza o estado
3. Ao final:
   - realiza o flush do estado final
   - salva o bitstream em `.bin`

As tabelas utilizadas são:

- `A_TABLE`
- `B_TABLE`
- `A_BITS`
- `B_BITS`
- `A_NBITS`
- `B_NBITS`

Cada símbolo possui:

- próximo estado
- quantidade de bits emitidos
- valor dos bits emitidos

---

# Funcionamento do Decoder

O decoder:

1. Lê o bitstream reversamente
2. Recupera o estado final usando `FLUSH_BITS`
3. Reconstrói os símbolos utilizando:
   - Reverse Lookup Table (RLT)
4. Atualiza os estados até recuperar a sequência original

---

# Estrutura do Projeto

```text
Projeto-ANS-main/
│
├── Encoder_Ans.c
├── Decoder.c
├── ANS-Streaming Encoder.py
├── ANS-Streaming-Decoder.py
├── Bit_Planes.py
├── inputs/
│   ├── input_1.txt ... input_8.txt
├── plano_bit_0.txt ... plano_bit_7.txt
├── input.txt
└── README.md
```

---

# Comandos de Teste Individuais

## Inputs `.txt` (pasta `inputs/`)

### input_1.txt

```bash
cp inputs/input_1.txt input.txt && python3 "ANS-Streaming Encoder.py" && python3 ANS-Streaming-Decoder.py
```

### input_2.txt

```bash
cp inputs/input_2.txt input.txt && python3 "ANS-Streaming Encoder.py" && python3 ANS-Streaming-Decoder.py
```

### input_3.txt

```bash
cp inputs/input_3.txt input.txt && python3 "ANS-Streaming Encoder.py" && python3 ANS-Streaming-Decoder.py
```

### input_4.txt

```bash
cp inputs/input_4.txt input.txt && python3 "ANS-Streaming Encoder.py" && python3 ANS-Streaming-Decoder.py
```

### input_5.txt

```bash
cp inputs/input_5.txt input.txt && python3 "ANS-Streaming Encoder.py" && python3 ANS-Streaming-Decoder.py
```

### input_6.txt

```bash
cp inputs/input_6.txt input.txt && python3 "ANS-Streaming Encoder.py" && python3 ANS-Streaming-Decoder.py
```

### input_7.txt

```bash
cp inputs/input_7.txt input.txt && python3 "ANS-Streaming Encoder.py" && python3 ANS-Streaming-Decoder.py
```

### input_8.txt

```bash
cp inputs/input_8.txt input.txt && python3 "ANS-Streaming Encoder.py" && python3 ANS-Streaming-Decoder.py
```

---

## Bit Planes (gerados a partir de `foto.jpg`)

> Pré-requisito: gerar todos os planos com `python3 Bit_Planes.py`

### Plano 0 (LSB)

```bash
cp plano_bit_0.txt input.txt && python3 "ANS-Streaming Encoder.py" && python3 ANS-Streaming-Decoder.py
```

### Plano 1

```bash
cp plano_bit_1.txt input.txt && python3 "ANS-Streaming Encoder.py" && python3 ANS-Streaming-Decoder.py
```

### Plano 2

```bash
cp plano_bit_2.txt input.txt && python3 "ANS-Streaming Encoder.py" && python3 ANS-Streaming-Decoder.py
```

### Plano 3

```bash
cp plano_bit_3.txt input.txt && python3 "ANS-Streaming Encoder.py" && python3 ANS-Streaming-Decoder.py
```

### Plano 4

```bash
cp plano_bit_4.txt input.txt && python3 "ANS-Streaming Encoder.py" && python3 ANS-Streaming-Decoder.py
```

### Plano 5

```bash
cp plano_bit_5.txt input.txt && python3 "ANS-Streaming Encoder.py" && python3 ANS-Streaming-Decoder.py
```

### Plano 6

```bash
cp plano_bit_6.txt input.txt && python3 "ANS-Streaming Encoder.py" && python3 ANS-Streaming-Decoder.py
```

### Plano 7 (MSB)

```bash
cp plano_bit_7.txt input.txt && python3 "ANS-Streaming Encoder.py" && python3 ANS-Streaming-Decoder.py
```
