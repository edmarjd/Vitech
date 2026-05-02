import numpy as np

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


def test_D_rANS():
    symbol_counts = [3, 3, 2]
    initial_state = 17910
    num_symbols = 9

    state = initial_state
    print(f"{'Output':<10}{'State'}")

    for _ in range(num_symbols):
        symbol, new_state = D_rANS(state, symbol_counts)
        print(f"{symbol:<10}{state}")
        state = new_state

    print(f"\nFinal State: {state}")


test_D_rANS()
