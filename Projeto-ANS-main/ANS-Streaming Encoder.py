import numpy as np
import time
import os
import argparse

from context_model import ContextModel, NUM_CONTEXTS_1D

# ---------------------------------------------------------------------------
# rANS primitivos
# ---------------------------------------------------------------------------

def _C_rANS(s: int, state: int, counts: list[int]) -> int:
    """Codifica símbolo s no estado rANS. counts = [c0, c1]."""
    M    = counts[0] + counts[1]
    cs   = sum(counts[:s])   # frequência acumulada até s
    fs   = counts[s]
    return (state // fs) * M + cs + (state % fs)


def _encode_symbol(state: int, s: int, counts: list[int],
                   bitstream: list[int]) -> int:
    """
    Normaliza o estado e codifica o símbolo s.
    Emite bits para manter o estado no intervalo válido.
    """
    fs = counts[s]
    # Normalização: emite bits enquanto estado >= 2 * f_s
    while state >= 2 * fs:
        bitstream.append(state & 1)
        state >>= 1
    return _C_rANS(s, state, counts)


# ---------------------------------------------------------------------------
# Encoder contextual — estado contínuo
# ---------------------------------------------------------------------------

def encode_stream(symbols: list[int], model: ContextModel,
                  recalc_window: int = 0) -> tuple[str, int]:
    n = len(symbols)
    if n == 0:
        return "", 0

    def get_ctx(i: int) -> int:
        return symbols[i + 1] if i + 1 < n else 0

    ctx0    = get_ctx(0)
    counts0 = model.get_counts(ctx0, 0)
    state   = counts0[0] + counts0[1]
    bitstream: list[int] = []

    for i in range(n):
        ctx    = get_ctx(i)
        blk    = i // recalc_window if recalc_window > 0 else None
        counts = model.get_counts(ctx, blk)
        state  = _encode_symbol(state, symbols[i], counts, bitstream)

    return "".join(map(str, bitstream)), state


# ---------------------------------------------------------------------------
# Leitura de input
# ---------------------------------------------------------------------------

def load_symbols(path: str) -> list[int]:
    with open(path, "r") as f:
        content = f.read().replace(",", " ").replace("\n", " ").replace("\r", "")
    tokens = content.split()
    if all(len(t) == 1 for t in tokens if t):
        return [int(x) for x in tokens if x in ("0", "1")]
    return [int(c) for c in content if c in ("0", "1")]


# ---------------------------------------------------------------------------
# Salvar arquivo comprimido
# ---------------------------------------------------------------------------

def save_encoded(path: str, num_symbols: int, mode: str, recalc_window: int,
                 bitstream: str, final_state: int, headers_hex: str) -> None:
    """
    Formato do arquivo comprimido (texto, um campo por linha):
      Linha 0: num_symbols
      Linha 1: mode
      Linha 2: recalc_window
      Linha 3: bitstream (string de 0s e 1s)
      Linha 4: final_state
      Linha 5: headers_hex
    """
    with open(path, "w") as f:
        f.write(f"{num_symbols}\n")
        f.write(f"{mode}\n")
        f.write(f"{recalc_window}\n")
        f.write(f"{bitstream}\n")
        f.write(f"{final_state}\n")
        f.write(f"{headers_hex}\n")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="ANS Streaming Encoder — Contextual")
    parser.add_argument("--input",         default="input.txt",        help="Arquivo de entrada")
    parser.add_argument("--output",        default="input_encoded.bin", help="Arquivo de saída")
    parser.add_argument("--mode",          default="static", choices=["static"])
    parser.add_argument("--recalc-window", type=int, default=1000,     help="Janela de recalibração")
    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f"Erro: arquivo '{args.input}' não encontrado.")
        return

    print("=" * 72)
    print(f"  ANS STREAMING ENCODER  |  mode={args.mode}  |  "
          f"recalc_window={args.recalc_window}")
    print("=" * 72)

    symbols = load_symbols(args.input)
    # NOTA CONTABILIDADE: 'num_symbols_real' é o tamanho SEM padding.
    # A versão anterior usava o bloco com padding como denominador, inflando
    # a taxa reportada. Aqui usamos apenas o tamanho real da entrada.
    num_symbols_real = len(symbols)
    print(f"Símbolos lidos: {num_symbols_real}")

    # Contexto 1D (2 ctx) — lookahead
    n_ctx = NUM_CONTEXTS_1D
    print(f"Contexto: 1D (lookahead-1)  |  {n_ctx} contextos")

    # Pré-varredura por bloco
    model = ContextModel(mode="static", num_contexts=n_ctx,
                         recalc_window=args.recalc_window)

    n = len(symbols)
    rw = args.recalc_window
    n_blocks = (n + rw - 1) // rw

    def get_ctx_prescan(i: int) -> int:
        return symbols[i + 1] if i + 1 < n else 0

    block_freqs = []
    for b in range(n_blocks):
        counts = [[1, 1] for _ in range(n_ctx)]
        start = b * rw
        end   = min(start + rw, n)
        for i in range(start, end):
            ctx = get_ctx_prescan(i)
            counts[ctx][symbols[i]] += 1
        block_freqs.append(model._normalize(counts))
    model.set_block_freqs(block_freqs)
    print(f"Blocos: {n_blocks} (recalc_window={rw})")

    t0 = time.time()
    bitstream, final_state = encode_stream(symbols, model, recalc_window=rw)
    t1 = time.time()

    headers_hex = model.serialize_static_headers()

    # Calcula métricas
    bits_bitstream   = len(bitstream)
    bits_state       = final_state.bit_length() if final_state > 0 else 1
    bits_header_oh   = model.header_overhead_bits()
    # CONTABILIDADE CORRETA: denominador = num_symbols_real (sem padding)
    # Overhead do header é parte do custo total transmitido
    bits_total_tx    = bits_bitstream + bits_state + bits_header_oh
    taxa_compressao  = (1.0 - bits_total_tx / num_symbols_real) * 100 \
                       if num_symbols_real > 0 else 0.0

    # Entropia global
    c0 = symbols.count(0); c1 = symbols.count(1)
    p0 = c0 / num_symbols_real; p1 = c1 / num_symbols_real
    H  = 0.0
    if p0 > 0: H -= p0 * np.log2(p0)
    if p1 > 0: H -= p1 * np.log2(p1)

    print(f"\nEntropia global H = {H:.6f} bits/símbolo")
    print(f"Limite teórico (sem contexto): {H * num_symbols_real:.1f} bits totais\n")

    print("-" * 72)
    print(f"{'Métrica':<40} {'Valor':>12} {'Unidade'}")
    print("-" * 72)
    print(f"{'Símbolos originais (sem padding)':<40} {num_symbols_real:>12} bits")
    print(f"{'Bits no bitstream rANS':<40} {bits_bitstream:>12} bits")
    print(f"{'Bits do estado final':<40} {bits_state:>12} bits")
    print(f"{'Overhead de cabeçalhos':<40} {bits_header_oh:>12} bits")
    print(f"{'Total transmitido':<40} {bits_total_tx:>12} bits")
    # CONTABILIDADE CORRETA (comentário)
    # taxa = (original - total_tx) / original   ← usa original SEM padding
    print(f"{'Taxa de compressão':<40} {taxa_compressao:>11.2f}%")
    print(f"{'Tempo de codificação':<40} {t1 - t0:>11.6f}s")
    print("-" * 72)

    save_encoded(args.output, num_symbols_real, args.mode, args.recalc_window,
                 bitstream, final_state, headers_hex)

    print(f"\nArquivo salvo: '{args.output}'")
    print("=" * 72)


if __name__ == "__main__":
    main()
