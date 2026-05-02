import numpy as np
import time

def C_rANS(s, state, symbol_counts):
    total_counts = np.sum(symbol_counts)  # Representa M
    cumul_counts = np.insert(np.cumsum(symbol_counts), 0, 0)  # Frequências acumuladas
    s_count = symbol_counts[s]
    next_state = (state // s_count) * total_counts + cumul_counts[s] + (state % s_count)
    return next_state

def Streaming_rANS_encoder(s_input, symbol_counts, range_factor, low_level=1):
    total_counts = np.sum(symbol_counts)  # Representa M
    bitstream = []  # Fluxo de bits inicializado
    state = low_level * total_counts  # Estado inicial

    num_symbols = len(s_input)

    # Cabeçalho
    print(f"{'Input':<6}{'State':<10}{'Stream op'}")
    for s in s_input:  # Itera sobre a sequência de símbolos
        stream_op = ""  # Operações no stream

        # Ajusta o estado e gera bits para mantê-lo dentro do intervalo
        while state >= range_factor * symbol_counts[s]:
            bit = state % 2
            bitstream.append(bit)
            stream_op += str(bit)
            state //= 2

        state = C_rANS(s, state, symbol_counts)

        print(f"{s:<6}{state:<10}{stream_op}")

    # Calcula comprimento médio do código e entropia
    bitstream_list = list(bitstream)
    average_codelength = len(bitstream_list) / num_symbols

    probabilities = np.array(symbol_counts) / total_counts
    entropy = -np.sum(probabilities * np.log2(probabilities))

    # Retorna estado final e o fluxo de bits
    return state, ''.join(map(str, bitstream)), num_symbols, average_codelength, entropy

# Parâmetros do exemplo
symbol_counts = [3, 3, 2]
# Gerar a sequência de 1000 símbolos: 50 repetições de (19 zeros seguidos de um 1)
s_input = [[0] * 19 + [1] for _ in range(50)]
s_input = [item for sublist in s_input for item in sublist]
range_factor = 2

# Medir tempo de execução
start_time = time.time()

# Executa o encoder
final_state, bitstream, num_symbols, average_codelength, entropy = Streaming_rANS_encoder(
    s_input, symbol_counts, range_factor
)

end_time = time.time()
execution_time = end_time - start_time

# Resultados finais
print("\nFinal State:", final_state)
print("BitStream:", bitstream)
print("Number of input symbols:", num_symbols)
print("Average codelength:", average_codelength)
print("Entropy:", entropy)
print("Execution time (seconds):", execution_time)
