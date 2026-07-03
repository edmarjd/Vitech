import numpy as np
import importlib, sys, os

# Bug fix: carrega ANS-Encoder.py dinamicamente (hífen no nome impede import normal)
_enc_path = os.path.join(os.path.dirname(__file__), "ANS-Encoder.py")
_spec = importlib.util.spec_from_file_location("ANS_Encoder", _enc_path)
_mod  = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_mod)
test_rANS_encoding = _mod.test_rANS_encoding


def D_rANS(state, symbol_counts):
    total_counts = np.sum(symbol_counts)  # Represents M
    cumul_counts = np.insert(np.cumsum(symbol_counts), 0, 0)  # The cumulative frequencies

    def cumul_inverse(y):
        for i, _s in enumerate(cumul_counts):
            if y < _s:
                return i - 1

    slot = state % total_counts  # Compute the slot
    s = cumul_inverse(slot)  # Decode the symbol
    prev_state = (state // total_counts) * symbol_counts[s] + slot - cumul_counts[s]  # Update the state

    return s, prev_state


def test_D_rANS(initial_state, num_symbols, symbol_counts):

    state = initial_state
    print(f"{'Output':<10}{'State'}")

    for _ in range(num_symbols):
        symbol, new_state = D_rANS(state, symbol_counts)
        print(f"{symbol:<10}{state}")
        state = new_state

    print(f"\nFinal State: {state}")


if __name__ == "__main__":
    symbol_counts = [3, 3, 2]
    input_symbols  = [0, 1, 0, 2, 2, 0, 2, 1, 2]

    print("=== ENCODER ===")
    final_state = test_rANS_encoding(symbol_counts, input_symbols)

    print("\n=== DECODER ===")
    # Bug fix: usa o estado dinâmico do encoder em vez de valor hardcoded
    test_D_rANS(final_state, len(input_symbols), symbol_counts)
