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
    # Bug fix: evita log2(0) que gera -inf e nan
    entropy = -np.sum(p * np.log2(p) for p in probabilities if p > 0)
    return entropy

# Função para rodar testes — retorna o estado final para o decoder
def test_rANS_encoding(symbol_counts, input_symbols):
    state = 0  # Estado inicial
    print(f"{'Input':<10}{'State':<10}")
    print("-" * 20)

    for s in input_symbols:
        state = C_rANS(s, state, symbol_counts)
        print(f"{s:<10}{state:<10}")

    num_symbols = len(input_symbols)
    entropy = calculate_entropy(symbol_counts)
    avg_codelength = int(state).bit_length() / num_symbols

    print(f"\nFinal State: {state}")
    print(f"Number of input symbols: {num_symbols}")
    print(f"Average codelength: {avg_codelength}")
    print(f"Entropy: {entropy}")
    return int(state)  # Bug fix: retorna estado final para uso no decoder

# Exemplo
symbol_counts = [3, 3, 2]  # Frequências
input_symbols = [0, 1, 0, 2, 2, 0, 2, 1, 2]  # Sequência de entrada

final_state = test_rANS_encoding(symbol_counts, input_symbols)
