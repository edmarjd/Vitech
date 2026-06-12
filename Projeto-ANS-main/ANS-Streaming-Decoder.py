import numpy as np
import os

def D_rANS(state, symbol_counts):
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


def Streaming_rANS_decoder(final_state, bitstream, symbol_counts, num_symbols):
    total_counts = np.sum(symbol_counts)  # Representa M
    bitstream = list(map(int, bitstream[::-1]))

    decoded_symbols = []
    states = []
    state = final_state

    for _ in range(num_symbols):
        s_decoded, prev_state = D_rANS(state, symbol_counts)
        decoded_symbols.append(s_decoded)
        states.append(state)

        while prev_state < total_counts and bitstream:
            bit = bitstream.pop(0)
            prev_state = (prev_state << 1) | bit

        state = prev_state

    initial_state = state
    return decoded_symbols, states, initial_state


# Carregar bitstream e metadados
if os.path.exists('input_encoded.bin'):
    with open('input_encoded.bin', 'r') as f:
        lines = f.readlines()
        final_state = int(lines[0].strip())
        bitstream = lines[1].strip()
        num_symbols = int(lines[2].strip())
        symbol_counts = [int(x) for x in lines[3].strip().split(',')]
else:
    # Fallback
    symbol_counts = [3, 3, 2]
    final_state = 14
    bitstream = "0100000111000111"
    num_symbols = 9

decoded_symbols, states, Final_state = Streaming_rANS_decoder(final_state, bitstream, symbol_counts, num_symbols)

# Decodificando na ordem inversa (ANS decodifica do fim para o começo)
decoded_symbols = decoded_symbols[::-1]

print(f"{'Decoded Symbol':<15}{'State':<10}")
for symbol, state in zip(decoded_symbols, states):
    print(f"{symbol:<15}{state:<10}")

print(f"\nFinal State: {Final_state}")

# Salvar resultado em output.txt
with open('output.txt', 'w') as f:
    f.write(' '.join(map(str, decoded_symbols)))

print("\nArquivo decodificado e salvo em output.txt")
