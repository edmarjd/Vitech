import numpy as np
import time
import os

# Configuração global do tamanho do bloco
BLOCK_SIZE = 64

def C_rANS(s, state, symbol_counts):
    total_counts = np.sum(symbol_counts)  # Representa M
    cumul_counts = np.insert(np.cumsum(symbol_counts), 0, 0)  # Frequências acumuladas
    s_count = symbol_counts[s]
    next_state = (state // s_count) * total_counts + cumul_counts[s] + (state % s_count)
    return next_state

def scale_frequencies(c0, c1, M, B):
    """
    Escalona as contagens de símbolos c0 e c1 de um bloco de tamanho B
    para frequências f0 e f1 que somem exatamente M, garantindo que f0, f1 >= 1.
    """
    if c0 == 0:
        return [1, M - 1]
    if c1 == 0:
        return [M - 1, 1]
    
    f0 = int(round(c0 * M / B))
    if f0 < 1:
        f0 = 1
    elif f0 > M - 1:
        f0 = M - 1
    f1 = M - f0
    return [f0, f1]

def calculate_block_entropy(block):
    """
    Calcula a entropia empírica de Shannon para um bloco de símbolos binários.
    """
    B = len(block)
    if B == 0:
        return 0.0
    c0 = block.count(0)
    c1 = block.count(1)
    p0 = c0 / B
    p1 = c1 / B
    entropy = 0.0
    if p0 > 0:
        entropy -= p0 * np.log2(p0)
    if p1 > 0:
        entropy -= p1 * np.log2(p1)
    return entropy

def Streaming_rANS_encoder_block(block_symbols, symbol_counts, range_factor=2, low_level=1):
    """
    Executa o codificador rANS de fluxo para um único bloco de símbolos.
    Retorna o estado final e a string do bitstream emitido para o bloco.
    """
    total_counts = np.sum(symbol_counts)  # Representa M
    bitstream = []  # Fluxo de bits do bloco
    state = low_level * total_counts  # Estado inicial

    for s in block_symbols:
        # Ajusta o estado e gera bits para mantê-lo dentro do intervalo [M, 2M - 1]
        while state >= range_factor * symbol_counts[s]:
            bit = state % 2
            bitstream.append(bit)
            state //= 2
        state = C_rANS(s, state, symbol_counts)

    bitstream_str = ''.join(map(str, bitstream))
    return int(state), bitstream_str

def main():
    print("=========================================================================")
    print(f"       INICIANDO CODIFICADOR ANS EM BLOCOS (Tamanho do Bloco = {BLOCK_SIZE})")
    print("=========================================================================")

    # Carregar entrada do arquivo input.txt
    if os.path.exists('input.txt'):
        with open('input.txt', 'r') as f:
            content = f.read().replace(',', ' ').split()
            s_input = [int(x) for x in content if x in ['0', '1']]
    else:
        # Fallback de demonstração
        print("Aviso: 'input.txt' não encontrado. Usando sequência de demonstração.")
        s_input = [0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1]

    num_symbols = len(s_input)
    print(f"Símbolos lidos da entrada: {num_symbols}")

    # Divisão em blocos com preenchimento (padding) se necessário
    remainder = num_symbols % BLOCK_SIZE
    if remainder != 0:
        padding_len = BLOCK_SIZE - remainder
        # Preenche com 0s
        padded_input = s_input + [0] * padding_len
        has_padding = True
    else:
        padded_input = s_input
        padding_len = 0
        has_padding = False

    # Dividir em blocos de tamanho BLOCK_SIZE
    blocks = [padded_input[i : i + BLOCK_SIZE] for i in range(0, len(padded_input), BLOCK_SIZE)]
    print(f"Quantidade de blocos criados: {len(blocks)}")
    if has_padding:
        print(f"-> Último bloco completado com {padding_len} símbolos de preenchimento (0).")

    # Processamento independente de cada bloco
    block_stats = []
    global_bitstream_parts = []
    final_states = []
    bitstream_lengths = []
    symbol_counts_list = []

    start_time = time.time()

    for idx, block in enumerate(blocks):
        B = len(block)
        # Fórmula: M = floor(0.99 * tamanho_do_bloco)
        M = int(np.floor(0.99 * B))
            
        c0 = block.count(0)
        c1 = block.count(1)
        
        # Escalonamento de frequências
        symbol_counts = scale_frequencies(c0, c1, M, B)
        
        # Codificação rANS para o bloco
        final_state, block_bitstream = Streaming_rANS_encoder_block(block, symbol_counts)
        
        # Estatísticas do bloco
        entropy = calculate_block_entropy(block)
        bits_originais = B
        # Bits comprimidos = tamanho do bitstream gerado + número de bits para representar o estado final
        bits_comprimidos = len(block_bitstream) + final_state.bit_length()
        taxa_compressao = ((bits_originais - bits_comprimidos) / bits_originais) * 100 if bits_originais > 0 else 0
        
        block_stats.append({
            'bloco': idx + 1,
            'tamanho': B,
            'M': M,
            'entropia': entropy,
            'bits_originais': bits_originais,
            'bits_comprimidos': bits_comprimidos,
            'taxa_compressao': taxa_compressao,
            'final_state': final_state,
            'bitstream': block_bitstream,
            'symbol_counts': symbol_counts
        })
        
        global_bitstream_parts.append(block_bitstream)
        final_states.append(final_state)
        bitstream_lengths.append(len(block_bitstream))
        symbol_counts_list.append(symbol_counts)

    execution_time = time.time() - start_time

    # Concatenação dos bitstreams individuais para formar o bitstream global
    global_bitstream = ''.join(global_bitstream_parts)

    # Cálculo da entropia empírica global da sequência original (sem padding)
    total_c0 = s_input.count(0)
    total_c1 = s_input.count(1)
    p0_global = total_c0 / num_symbols if num_symbols > 0 else 0
    p1_global = total_c1 / num_symbols if num_symbols > 0 else 0
    total_entropy = 0.0
    if p0_global > 0:
        total_entropy -= p0_global * np.log2(p0_global)
    if p1_global > 0:
        total_entropy -= p1_global * np.log2(p1_global)

    # Totais para a tabela
    total_bits_originais = sum(b['bits_originais'] for b in block_stats)
    total_bits_comprimidos = sum(b['bits_comprimidos'] for b in block_stats)
    global_compression_rate = ((total_bits_originais - total_bits_comprimidos) / total_bits_originais) * 100 if total_bits_originais > 0 else 0

    # Imprimir tabela de análise
    print(" " + "="*85)
    print(f"{'TABELA DE ANÁLISE DOS BLOCOS':^85}")
    print("="*85)
    print(f"{'Bloco':<8}{'Tamanho':<10}{'M':<6}{'Entropia (H)':<15}{'Bits Originais':<16}{'Bits Comprimidos':<18}{'Taxa Comp.':<12}")
    print("-"*85)
    for b in block_stats:
        print(f"{b['bloco']:<8}{b['tamanho']:<10}{b['M']:<6}{b['entropia']:<15.4f}{b['bits_originais']:<16}{b['bits_comprimidos']:<18}{b['taxa_compressao']:<12.2f}%")
    print("-"*85)
    print(f"{'TOTAL':<8}{total_bits_originais:<10}{'-':<6}{total_entropy:<15.4f}{num_symbols:<16}{total_bits_comprimidos:<18}{global_compression_rate:<12.2f}%")
    print("="*85)

    # Imprimir resumo final com a entropia total
    print(" " + "="*85)
    print(f"{'RESUMO FINAL':^85}")
    print("="*85)
    print(f"- Símbolos originais na entrada: {num_symbols}")
    print(f"- Tamanho selecionado do bloco: {BLOCK_SIZE}")
    print(f"- Quantidade total de blocos processados: {len(blocks)}")
    if has_padding:
        print(f"- Bits de preenchimento (padding) adicionados: {padding_len} bits")
    print(f"- Entropia total empírica (da entrada): {total_entropy:.6f} bits/símbolo")
    print(f"- Bits originais totais (sem preenchimento): {num_symbols} bits")
    print(f"- Bits comprimidos totais (incluindo estado final de cada bloco): {total_bits_comprimidos} bits")
    print(f"- Taxa de compressão global: {global_compression_rate:.2f}%")
    print(f"- Tempo total de execução do codificador: {execution_time:.6f} segundos")

    # Salvar bitstream e metadados estruturados para o decoder
    with open('input_encoded.bin', 'w') as f:
        f.write(f"{num_symbols}\n")
        f.write(f"{BLOCK_SIZE}\n")
        f.write(f"{global_bitstream}\n")
        f.write(",".join(map(str, final_states)) + "\n")
        f.write(",".join(map(str, bitstream_lengths)) + "\n")
        counts_str = ";".join(f"{sc[0]},{sc[1]}" for sc in symbol_counts_list)
        f.write(counts_str + "\n")

    print(" " + "="*85)
    print(f"{'Arquivo codificado e metadados salvos em \'input_encoded.bin\' com sucesso!':^85}")
    print("="*85)

if __name__ == "__main__":
    main()
