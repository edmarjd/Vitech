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
├── input.txt
├── encoder
├── decoder
└── README.md

TESTE: 
./venv/bin/python3 generate_test.py && mv teste_entropy.txt input.txt && ./venv/bin/python3 "ANS-Streaming Encoder.py"
