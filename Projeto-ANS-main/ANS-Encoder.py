import numpy as np

def C_rANS(s, state, symbol_counts):
    total_counts = np.sum(symbol_counts)  # Representa M
    cumul_counts = np.insert(np.cumsum(symbol_counts), 0, 0)  # Frequências acumuladas

    s_count = symbol_counts[s]  # Frequência do símbolo atual
    next_state = (state // s_count) * total_counts + cumul_counts[s] + (state % s_count)
    return next_state

# Função para calcular entropia
def calculate_entropy(symbol_counts):
    total_counts = np.sum(symbol_counts)
    probabilities = np.array(symbol_counts) / total_counts
    entropy = -np.sum(probabilities * np.log2(probabilities))
    return entropy

# Função para rodar testes
def test_rANS_encoding(symbol_counts, input_symbols):
    state = 0  # Estado inicial
    print(f"{'Input':<10}{'State':<10}")
    print("-" * 20)

    for s in input_symbols:
        state = C_rANS(s, state, symbol_counts)
        print(f"{s:<10}{state:<10}")

    num_symbols = len(input_symbols)
    entropy = calculate_entropy(symbol_counts)
    avg_codelength = int(state).bit_length() / num_symbols  # Converter state para int antes de calcular o comprimento

    print(f"\nFinal State: {state}")
    print(f"Number of input symbols: {num_symbols}")
    print(f"Average codelength: {avg_codelength}")
    print(f"Entropy: {entropy}")

# Exemplo
symbol_counts = [3, 3, 2]  # Frequências
input_symbols = [0, 1, 0, 2, 2, 0, 2, 1, 2]  # Sequência de entrada

test_rANS_encoding(symbol_counts, input_symbols)
