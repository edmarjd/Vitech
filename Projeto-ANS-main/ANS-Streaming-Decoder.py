import numpy as np
import os

def D_rANS(state, symbol_counts):
    """
    Decodifica um único símbolo utilizando rANS e retorna o símbolo decodificado
    e o estado anterior (não normalizado).
    """
    total_counts = np.sum(symbol_counts)  # Representa M
    cumul_counts = np.insert(np.cumsum(symbol_counts), 0, 0)  # Frequências acumuladas

    def cumul_inverse(y):
        for i, _s in enumerate(cumul_counts):
            if y < _s:
                return i - 1

    slot = state % total_counts
    s = cumul_inverse(slot)
    prev_state = (state // total_counts) * symbol_counts[s] + slot - cumul_counts[s]
    return s, prev_state


def Streaming_rANS_decoder_block(final_state, block_bitstream_str, symbol_counts, num_symbols_in_block):
    """
    Decodifica um único bloco codificado com rANS de fluxo.
    Retorna a lista de símbolos decodificados para este bloco.
    """
    total_counts = np.sum(symbol_counts)  # Representa M
    # Inverte o bitstream pois o rANS decodifica na ordem reversa da escrita (LIFO)
    bitstream = list(map(int, block_bitstream_str[::-1]))

    decoded_symbols = []
    state = final_state

    for _ in range(num_symbols_in_block):
        s_decoded, prev_state = D_rANS(state, symbol_counts)
        decoded_symbols.append(s_decoded)

        # Consome bits do stream para reconstruir o estado até que prev_state >= M
        while prev_state < total_counts and bitstream:
            bit = bitstream.pop(0)
            prev_state = (prev_state << 1) | bit

        state = prev_state

    # Como o rANS decodifica na ordem reversa, invertemos os símbolos decodificados para a ordem original
    return decoded_symbols[::-1]


def main():
    print("=========================================================================")
    print("                    DECODIFICADOR ANS EM BLOCOS")
    print("=========================================================================")

    # Carregar bitstream e metadados estruturados
    if os.path.exists('input_encoded.bin'):
        with open('input_encoded.bin', 'r') as f:
            lines = f.readlines()
            num_symbols = int(lines[0].strip())
            block_size = int(lines[1].strip())
            global_bitstream = lines[2].strip()
            
            final_states = [int(x) for x in lines[3].strip().split(',')]
            bitstream_lengths = [int(x) for x in lines[4].strip().split(',')]
            
            symbol_counts_list = []
            for pair_str in lines[5].strip().split(';'):
                symbol_counts_list.append([int(x) for x in pair_str.split(',')])
    else:
        print("Erro: Arquivo 'input_encoded.bin' não encontrado. Abortando decodificação.")
        return

    print(f"Símbolos originais para decodificar: {num_symbols}")
    print(f"Tamanho do bloco: {block_size}")
    print(f"Quantidade total de blocos codificados: {len(final_states)}")
    print(f"Tamanho do bitstream global: {len(global_bitstream)} bits")

    decoded_sequence = []
    bit_idx = 0

    print("\n" + "-"*65)
    print(f"{'Bloco':<8}{'Tamanho':<10}{'Estado Final':<15}{'Comp. Bitstream':<18}{'Counts (f0,f1)':<15}")
    print("-"*65)

    for idx, (final_state, bit_len, symbol_counts) in enumerate(zip(final_states, bitstream_lengths, symbol_counts_list)):
        # Extrai a porção específica do bitstream global correspondente a este bloco
        block_bitstream = global_bitstream[bit_idx : bit_idx + bit_len]
        bit_idx += bit_len
        
        # Cada bloco completo tem exatamente block_size símbolos (incluindo o padding)
        num_symbols_in_block = block_size
        
        # Decodifica o bloco individualmente
        decoded_block = Streaming_rANS_decoder_block(final_state, block_bitstream, symbol_counts, num_symbols_in_block)
        decoded_sequence.extend(decoded_block)
        
        print(f"{idx+1:<8}{block_size:<10}{final_state:<15}{bit_len:<18}{str(symbol_counts):<15}")

    print("-"*65)

    # Remove os símbolos extras de preenchimento (padding) adicionados durante a codificação
    decoded_sequence = decoded_sequence[:num_symbols]

    # Salvar resultado decodificado em output.txt
    with open('output.txt', 'w') as f:
        f.write(' '.join(map(str, decoded_sequence)))

    print(f"\nSequência decodificada salva em 'output.txt' ({len(decoded_sequence)} símbolos).")

    # Verificação de integridade comparando com input.txt original
    if os.path.exists('input.txt'):
        with open('input.txt', 'r') as f:
            content = f.read().replace('\n', ' ').split()
            original_symbols = [int(x) for x in content if x in ['0', '1']]
        
        if decoded_sequence == original_symbols:
            print("\n" + "="*65)
            print("  SUCESSO: A sequência decodificada é 100% IDÊNTICA à original!")
            print("="*65)
        else:
            print("\n" + "!"*65)
            print("  ERRO: A sequência decodificada é DIFERENTE da original!")
            print("!"*65)
            # Exibir as diferenças se houver
            diff_indices = [i for i, (x, y) in enumerate(zip(original_symbols, decoded_sequence)) if x != y]
            print(f"Total de divergências: {len(diff_indices)} símbolos.")
            print(f"Primeiras divergências nas posições: {diff_indices[:10]}")
    else:
        print("\nAviso: 'input.txt' não encontrado para comparação de integridade.")

if __name__ == "__main__":
    main()
