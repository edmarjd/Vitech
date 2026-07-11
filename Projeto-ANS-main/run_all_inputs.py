"""
run_all_inputs.py  —  Versão 2.0 (Tabela Comparativa — 3 Modos)
---------------------------------------------------------------
Executa o encoder/decoder contextual v2 nos 8 inputs (INPUT_1..INPUT_8)
e gera tabela comparativa com:
  - static  RECALC_WINDOW=1000
  - static  RECALC_WINDOW=2000
  - static  RECALC_WINDOW=4000
  - adaptive (estatísticas globais, zero overhead por janela)

Mudanças em relação à versão original:
- Ponto 1: usa encoder com estado rANS contínuo (não reiniciado por bloco).
- Ponto 2/3: modelo de contexto de ordem 1 integrado.
- Ponto 4: contabilidade de padding corrigida (denominador = símbolos reais).
- Adiciona colunas novas à planilha sem quebrar as existentes.
"""

import numpy as np
import time
import os
import csv
import shutil
from collections import deque

from context_model import ContextModel, NUM_CONTEXTS_1D

# ---------------------------------------------------------------------------
# Configuração
# ---------------------------------------------------------------------------

INPUT_DIR = "inputs"
CSV_FILE  = "Book 1(Planilha1).csv"

MODES = [
    ("static",   1000),
    ("static",   2000),
    ("static",   4000),
    ("adaptive", 0),
]

# ---------------------------------------------------------------------------
# rANS primitivos (inlinados para independência de arquivo)
# ---------------------------------------------------------------------------

def _C_rANS(s: int, state: int, counts: list) -> int:
    M  = counts[0] + counts[1]
    cs = [0, counts[0]]
    return (state // counts[s]) * M + cs[s] + (state % counts[s])


def _encode_symbol(state: int, s: int, counts: list, bitstream: list) -> int:
    fs = counts[s]
    while state >= 2 * fs:
        bitstream.append(state & 1)
        state >>= 1
    return _C_rANS(s, state, counts)


def _D_rANS(state: int, counts: list) -> tuple:
    M    = counts[0] + counts[1]
    cs   = [0, counts[0]]
    slot = state % M
    s    = 1 if slot >= counts[0] else 0
    prev = (state // M) * counts[s] + slot - cs[s]
    return s, prev


# ---------------------------------------------------------------------------
# Pipeline encoder contextual (inline)
# ---------------------------------------------------------------------------

def contextual_encode(symbols: list, model: ContextModel) -> tuple:
    """Retorna (bitstream_str, final_state)."""
    n = len(symbols)
    if n == 0:
        return "", 0
    ctx0    = symbols[1] if n > 1 else 0
    counts0 = model.get_counts(ctx0)
    state   = counts0[0] + counts0[1]
    bits: list = []
    for i in range(n):
        ctx    = symbols[i + 1] if i + 1 < n else 0
        counts = model.get_counts(ctx)
        state  = _encode_symbol(state, symbols[i], counts, bits)
        model.update_after_symbol(ctx, symbols[i])
    return "".join(map(str, bits)), state


def contextual_decode(bitstream_str: str, final_state: int,
                      num_symbols: int, model: ContextModel) -> list:
    """Decodifica com estado rANS contínuo e contexto lookahead."""
    bits_q   = deque(map(int, bitstream_str[::-1]))
    decoded  = [0] * num_symbols
    state    = final_state
    last_dec = 0

    for i in range(num_symbols - 1, -1, -1):
        ctx    = last_dec
        counts = model.get_counts(ctx)
        s, prev = _D_rANS(state, counts)
        M = counts[0] + counts[1]
        while prev < M and bits_q:
            prev = (prev << 1) | bits_q.popleft()
        state      = prev
        decoded[i] = s
        last_dec   = s
        model.advance_window_decoder()

    return decoded


# ---------------------------------------------------------------------------
# Leitura de input
# ---------------------------------------------------------------------------

def load_symbols(path: str) -> list:
    with open(path, "r") as f:
        content = f.read().replace(",", " ").replace("\n", " ").replace("\r", "")
    tokens = content.split()
    if all(len(t) == 1 for t in tokens if t):
        return [int(x) for x in tokens if x in ("0", "1")]
    return [int(c) for c in content if c in ("0", "1")]


# ---------------------------------------------------------------------------
# Pipeline completo para um input × um modo
# ---------------------------------------------------------------------------

def run_one(path: str, mode: str, recalc_window: int) -> dict:
    symbols = load_symbols(path)
    n = len(symbols)          # tamanho REAL, sem padding

    # Encoder
    model_enc = ContextModel(mode=mode, num_contexts=NUM_CONTEXTS_1D,
                             recalc_window=recalc_window)
    if mode == "adaptive":
        model_enc.set_global_frequencies(symbols)
    elif mode == "static":
        # Priming: usa varredura prévia para inicializar tabela antes de codificar.
        # Resolve o caso em que n < RECALC_WINDOW (janela nunca dispara).
        model_enc.prime_static_from_scan(symbols)

    t0 = time.time()
    bitstream, final_state = contextual_encode(symbols, model_enc)
    t1 = time.time()

    # Serializa cabeçalhos
    if mode == "static":
        hdr_hex = model_enc.serialize_static_headers()
    else:
        hdr_hex = model_enc.serialize_adaptive_header()

    # Decoder
    model_dec = ContextModel(mode=mode, num_contexts=NUM_CONTEXTS_1D,
                             recalc_window=recalc_window)
    if mode == "static":
        model_dec.load_static_headers(hdr_hex)
    else:
        model_dec.load_adaptive_header(hdr_hex)

    decoded = contextual_decode(bitstream, final_state, n, model_dec)
    ok = (decoded == symbols)

    # Métricas
    bits_bs    = len(bitstream)
    bits_state = final_state.bit_length() if final_state > 0 else 1
    bits_hdr   = model_enc.header_overhead_bits()
    bits_total = bits_bs + bits_state + bits_hdr

    # CONTABILIDADE CORRETA: denominador = n (sem padding)
    taxa = (1.0 - bits_total / n) * 100 if n > 0 else 0.0

    return {
        "n":          n,
        "bits_bs":    bits_bs,
        "bits_state": bits_state,
        "bits_hdr":   bits_hdr,
        "bits_total": bits_total,
        "taxa":       taxa,
        "decode_ok":  ok,
        "tempo_enc":  round(t1 - t0, 6),
    }


# ---------------------------------------------------------------------------
# Tabela no console
# ---------------------------------------------------------------------------

def print_comparison_table(all_results: dict) -> None:
    """
    Imprime tabela comparativa no formato:
      INPUT | n | mode/window | orig | total_tx | taxa | hdr_overhead | OK
    """
    sep = "=" * 108
    print("\n" + sep)
    print("  TABELA COMPARATIVA — ANS CONTEXTUAL v2 (contexto ordem 1, estado contínuo)")
    print(sep)
    print(f"{'INPUT':<10} {'n':>6} | {'Modo':<20} | {'Original':>9} | "
          f"{'Comprimido':>11} | {'Taxa':>8} | {'Overhead Hdr':>13} | {'Decode'}")
    print("-" * 108)

    for inp_name, modes_data in sorted(all_results.items()):
        first = True
        for mode_key, r in modes_data.items():
            inp_label = inp_name if first else ""
            n_label   = str(r["n"]) if first else ""
            first = False
            ok    = "✓ OK" if r["decode_ok"] else "✗ ERRO"
            print(f"{inp_label:<10} {n_label:>6} | {mode_key:<20} | "
                  f"{r['n']:>9} | {r['bits_total']:>11} | "
                  f"{r['taxa']:>7.2f}% | {r['bits_hdr']:>13} | {ok}")
        print("-" * 108)

    print(sep)

    # Resumo: quantos inputs atingem >60% em cada modo
    print("\n  Resumo por modo — inputs com taxa > 60%:")
    for (mode, rw) in MODES:
        mode_key = f"{mode}/{rw}" if mode == "static" else "adaptive"
        count = sum(
            1 for data in all_results.values()
            if data.get(mode_key, {}).get("taxa", 0) > 60.0
        )
        print(f"    {mode_key:<20}: {count}/8 inputs com >60%")
    print(sep + "\n")


# ---------------------------------------------------------------------------
# CSV update (mantém compatibilidade com colunas existentes)
# ---------------------------------------------------------------------------

def update_csv(all_results: dict) -> None:
    if not os.path.exists(CSV_FILE):
        print(f"Aviso: '{CSV_FILE}' não encontrado; CSV não atualizado.")
        return

    backup = CSV_FILE.replace(".csv", "_backup_v2.csv")
    shutil.copy2(CSV_FILE, backup)
    print(f"Backup salvo em '{backup}'")

    with open(CSV_FILE, "r", encoding="utf-8-sig", errors="replace") as f:
        rows = list(csv.reader(f, delimiter=";"))

    # Adiciona colunas novas no final de cada linha de métrica relevante
    # (sem modificar a estrutura existente, apenas apêndice)
    header_appended = False
    for row in rows:
        if row and "BLOCO" in row[0]:
            if not header_appended:
                for (mode, rw) in MODES:
                    mk = f"{mode}/{rw}" if mode == "static" else "adaptive"
                    row.extend([f"v2_{mk}_taxa", f"v2_{mk}_hdr_bits"])
                header_appended = True

    with open(CSV_FILE, "w", newline="", encoding="utf-8-sig") as f:
        csv.writer(f, delimiter=";").writerows(rows)

    print("CSV atualizado (colunas v2 adicionadas ao cabeçalho).")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    print("=" * 72)
    print("  RUN ALL INPUTS v2  —  ANS Contextual (ordem 1, estado contínuo)")
    print("=" * 72)

    all_results: dict = {}

    for i in range(1, 9):
        path = os.path.join(INPUT_DIR, f"input_{i}.txt")
        inp_name = f"INPUT_{i}"
        if not os.path.exists(path):
            print(f"Aviso: '{path}' não encontrado, pulando.")
            continue

        print(f"\n[{i}/8] {path}")
        all_results[inp_name] = {}

        for (mode, rw) in MODES:
            mode_key = f"{mode}/{rw}" if mode == "static" else "adaptive"
            try:
                r = run_one(path, mode, rw)
                all_results[inp_name][mode_key] = r
                ok = "✓" if r["decode_ok"] else "✗ ERRO"
                print(f"  {mode_key:<20}: taxa={r['taxa']:6.2f}%  "
                      f"orig={r['n']}b  total={r['bits_total']}b  "
                      f"hdr={r['bits_hdr']}b  {ok}")
            except Exception as e:
                print(f"  {mode_key:<20}: ERRO — {e}")
                all_results[inp_name][mode_key] = None

    print_comparison_table(all_results)

    # Atualiza CSV (opcional)
    try:
        update_csv(all_results)
    except Exception as e:
        print(f"Aviso: falha ao atualizar CSV — {e}")

    # Pipeline de imagem
    print("=" * 72)
    print("  IMAGEM — spatial_predictor.py (MED + rANS contextual 2D)")
    print("=" * 72)
    if os.path.exists("foto.jpg"):
        try:
            import spatial_predictor as sp
            import cv2

            img = cv2.imread("foto.jpg", cv2.IMREAD_GRAYSCALE)
            if img is not None:
                H, W = img.shape
                orig_bits = H * W * 8
                print(f"foto.jpg: {W}×{H} px, {orig_bits} bits originais\n")

                for mode in ["static", "adaptive"]:
                    out_enc = f"foto_encoded_{mode}.bin"
                    out_dec = f"foto_decoded_{mode}.png"
                    print(f"--- Modo: {mode} ---")
                    m = sp.encode_image("foto.jpg", out_enc, mode=mode, recalc_window=1000)
                    sp.decode_image(out_enc, out_dec)

                    rec = cv2.imread(out_dec, cv2.IMREAD_GRAYSCALE)
                    lossless = (rec is not None and np.array_equal(img, rec))
                    status = "✓ LOSSLESS" if lossless else "✗ ERRO"
                    print(f"  Taxa: {m['taxa']:.2f}%  |  {status}\n")
        except Exception as e:
            print(f"Erro no pipeline de imagem: {e}")
    else:
        print("'foto.jpg' não encontrada no diretório atual.")

    print("=" * 72)
    print("  Processamento concluído.")
    print("=" * 72)


if __name__ == "__main__":
    main()
