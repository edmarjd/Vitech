"""
spatial_predictor.py  —  Pipeline de Predição Espacial para Imagem
-------------------------------------------------------------------
Implementa a arquitetura alternativa para compressão de imagens descrita
no diagnóstico (Ponto 4): em vez de comprimir os 8 planos de bits
separadamente (que trava em ~25-30% porque os planos menos significativos
são ruído), este módulo:

  1. Prediz cada pixel com o previsor MED do JPEG-LS usando vizinhos já
     codificados (causalidade esquerda→direita, cima→baixo).
  2. Calcula o resíduo: residuo = pixel_real − previsao.
  3. Mapeia o resíduo para inteiro não-negativo via zig-zag encoding.
  4. Decompõe cada resíduo em 8 bits e codifica bit a bit com rANS
     contextual (contexto 2D: bit à esquerda do mesmo plano × bit acima).
  5. Usa estado rANS contínuo (não reiniciado por pixel/plano).

O decoder reconstrói pixel = previsao + residuo usando os vizinhos já
decodificados, na mesma ordem causal.

Uso:
    python spatial_predictor.py --encode foto.jpg [--output foto_encoded.bin]
    python spatial_predictor.py --decode foto_encoded.bin [--output foto_dec.png]
    python spatial_predictor.py --encode foto.jpg --decode foto_encoded.bin --verify
"""

import argparse
import os
import struct
import time
import numpy as np

try:
    import cv2
    _CV2_AVAILABLE = True
except ImportError:
    _CV2_AVAILABLE = False

from context_model import ContextModel, NUM_CONTEXTS_2D, PRECISION


# ---------------------------------------------------------------------------
# Previsor MED (JPEG-LS)
# ---------------------------------------------------------------------------

def med_predict(a: int, b: int, c: int) -> int:
    """
    Previsor MED (Median Edge Detection) do JPEG-LS.
      a = pixel à esquerda
      b = pixel acima
      c = pixel diagonal superior-esquerda
    Pixels de borda: a=128, b=128, c=128 (valor médio do intervalo [0,255]).
    """
    if c >= max(a, b):
        return min(a, b)
    elif c <= min(a, b):
        return max(a, b)
    else:
        return a + b - c


# ---------------------------------------------------------------------------
# Zig-zag encoding / decoding de resíduos
# ---------------------------------------------------------------------------

def zigzag_encode(r: int) -> int:
    """
    Mapeia resíduo r ∈ [-255, 255] → inteiro não-negativo n ∈ [0, 510].
    Padrão: 0→0, -1→1, 1→2, -2→3, 2→4, ...
    n = 2*|r| se r <= 0, else 2*r - 1
    """
    return (-2 * r - 1) if r < 0 else (2 * r)


def zigzag_decode(n: int) -> int:
    """Inverso de zigzag_encode."""
    return -(n + 1) // 2 if n % 2 else n // 2


# ---------------------------------------------------------------------------
# Extração de resíduos da imagem inteira
# ---------------------------------------------------------------------------

def compute_residuals(img: np.ndarray) -> np.ndarray:
    """
    Retorna matriz de resíduos zig-zag codificados, mesma shape de img.
    img deve ser uint8 grayscale (H × W).
    """
    H, W = img.shape
    residuals = np.zeros((H, W), dtype=np.int32)
    for r in range(H):
        for c in range(W):
            a = int(img[r, c - 1]) if c > 0 else 128
            b = int(img[r - 1, c]) if r > 0 else 128
            cc = int(img[r - 1, c - 1]) if (r > 0 and c > 0) else 128
            pred = med_predict(a, b, cc)
            res  = int(img[r, c]) - pred
            residuals[r, c] = zigzag_encode(res)
    return residuals


def reconstruct_image(residuals: np.ndarray) -> np.ndarray:
    """
    Reconstrói imagem a partir dos resíduos zig-zag, usando vizinhos já
    reconstruídos (mesma ordem causal esquerda→direita, cima→baixo).
    """
    H, W = residuals.shape
    img = np.zeros((H, W), dtype=np.uint8)
    for r in range(H):
        for c in range(W):
            a = int(img[r, c - 1]) if c > 0 else 128
            b = int(img[r - 1, c]) if r > 0 else 128
            cc = int(img[r - 1, c - 1]) if (r > 0 and c > 0) else 128
            pred = med_predict(a, b, cc)
            res  = zigzag_decode(int(residuals[r, c]))
            px   = np.clip(pred + res, 0, 255)
            img[r, c] = px
    return img


# ---------------------------------------------------------------------------
# Conversão resíduo → bits (para rANS binário)
# ---------------------------------------------------------------------------

def residuals_to_bits(residuals: np.ndarray) -> tuple[list[int], tuple]:
    """
    Achata os resíduos (H×W) e converte cada valor (0-510) em 9 bits.
    Retorna (lista_de_bits, shape).
    Usa 9 bits (valores de resíduo vão de 0 a 510 = 2×255).
    """
    flat = residuals.flatten().tolist()
    bits = []
    for v in flat:
        # 9 bits: MSB primeiro
        for b in range(8, -1, -1):
            bits.append((v >> b) & 1)
    return bits, residuals.shape


def bits_to_residuals(bits: list[int], shape: tuple) -> np.ndarray:
    """Inverso de residuals_to_bits."""
    H, W = shape
    n_pixels = H * W
    residuals = np.zeros(n_pixels, dtype=np.int32)
    for i in range(n_pixels):
        v = 0
        for b in range(9):
            v = (v << 1) | bits[i * 9 + b]
        residuals[i] = v
    return residuals.reshape(H, W)


# ---------------------------------------------------------------------------
# Contexto 2D para bits de resíduo
# ---------------------------------------------------------------------------

def compute_bit_contexts(bits: list[int], pixels_per_row: int, bits_per_pixel: int = 9) -> list[int]:
    """
    Calcula o contexto para cada bit.
    Contexto = (bit à esquerda no mesmo plano, bit acima no mesmo plano).
    Codificado como: ctx = bit_esq * 2 + bit_acima  ∈ {0,1,2,3}.

    'bits_per_pixel' é o número de bits por pixel (9 para resíduos 0-510).
    """
    total = len(bits)
    contexts = [0] * total
    row_bits = pixels_per_row * bits_per_pixel  # bits por linha de imagem

    for i in range(total):
        bit_in_pixel = i % bits_per_pixel   # qual bit dentro do pixel (0=MSB)
        pixel_idx    = i // bits_per_pixel
        col_pixel    = pixel_idx % pixels_per_row

        # bit à esquerda: mesmo plano de bit, pixel à esquerda (ou 0 na borda)
        if col_pixel > 0:
            esq = bits[i - bits_per_pixel]
        else:
            esq = 0

        # bit acima: mesmo plano de bit, pixel acima (ou 0 na borda)
        above_idx = i - row_bits
        if above_idx >= 0:
            acima = bits[above_idx]
        else:
            acima = 0

        contexts[i] = esq * 2 + acima
    return contexts


# ---------------------------------------------------------------------------
# rANS binário — encoder e decoder para bits
# ---------------------------------------------------------------------------

def _C_rANS(s: int, state: int, counts: list[int]) -> int:
    M = counts[0] + counts[1]
    cs = [0, counts[0]]
    fs = counts[s]
    return (state // fs) * M + cs[s] + (state % fs)


def encode_bits_contextual(bits: list[int], contexts: list[int],
                           model: ContextModel) -> tuple[str, int]:
    """Codifica lista de bits com contexto 2D, estado rANS contínuo."""
    n = len(bits)
    if n == 0:
        return "", 0

    ctx0    = contexts[0]
    counts0 = model.get_counts(ctx0)
    M0      = counts0[0] + counts0[1]
    state   = M0
    bitstream: list[int] = []

    for i in range(n):
        ctx    = contexts[i]
        counts = model.get_counts(ctx)
        fs     = counts[bits[i]]
        while state >= 2 * fs:
            bitstream.append(state & 1)
            state >>= 1
        state = _C_rANS(bits[i], state, counts)
        model.update_after_symbol(ctx, bits[i])

    return "".join(map(str, bitstream)), state


def decode_bits_contextual(bitstream_str: str, final_state: int, n_bits: int,
                           contexts: list[int], model: ContextModel) -> list[int]:
    """Decodifica bits com contexto 2D, estado rANS contínuo (loop reverso)."""
    from collections import deque
    bits_q = deque(map(int, bitstream_str[::-1]))
    decoded = [0] * n_bits
    state   = final_state

    for i in range(n_bits - 1, -1, -1):
        ctx    = contexts[i]
        counts = model.get_counts(ctx)
        M      = counts[0] + counts[1]
        slot   = state % M
        s      = 1 if slot >= counts[0] else 0
        cs     = [0, counts[0]]
        prev   = (state // M) * counts[s] + slot - cs[s]

        while prev < M and bits_q:
            prev = (prev << 1) | bits_q.popleft()

        state      = prev
        decoded[i] = s
        model.advance_window_decoder()

    return decoded


# ---------------------------------------------------------------------------
# Codificação / Decodificação da imagem completa
# ---------------------------------------------------------------------------

def encode_image(image_path: str, output_path: str, mode: str = "static",
                 recalc_window: int = 1000) -> dict:
    """
    Codifica uma imagem grayscale em JPEG-LS + rANS contextual.
    Retorna dicionário com métricas.
    """
    if not _CV2_AVAILABLE:
        raise ImportError("OpenCV (cv2) é necessário. Instale com: pip install opencv-python")

    img = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
    if img is None:
        raise FileNotFoundError(f"Imagem '{image_path}' não encontrada.")

    H, W = img.shape
    n_pixels = H * W

    print(f"Imagem: {W}×{H} px, {n_pixels} pixels, {n_pixels * 8} bits originais")

    # Passo 1: resíduos MED
    t0 = time.time()
    residuals = compute_residuals(img)
    bits, shape = residuals_to_bits(residuals)
    bits_per_pixel = 9
    # Contexto posicional: ctx = min(bit_in_pixel, 3)
    # Captura a posição do bit dentro do resíduo de 9 bits — bits mais
    # significativos (MSBs) têm distribuição muito diferente dos LSBs.
    contexts = [min(i % bits_per_pixel, 3) for i in range(len(bits))]
    t1 = time.time()
    print(f"Resíduos calculados em {t1 - t0:.3f}s | {len(bits)} bits a codificar")

    # Passo 2: modelo de contexto 2D (4 contextos posicionais)
    model = ContextModel(mode=mode, num_contexts=NUM_CONTEXTS_2D,
                         recalc_window=recalc_window)
    if mode == "adaptive":
        # Varredura prévia com contexto posicional
        counts_pre = [[1, 1] for _ in range(NUM_CONTEXTS_2D)]
        for b_val, ctx in zip(bits, contexts):
            counts_pre[ctx][b_val] += 1
        model._freq = model._normalize(counts_pre)
    elif mode == "static":
        # Priming com tabela única global — sem recalibração intermediária
        # (necessário para manter simetria com decodificação reversa)
        model.prime_static_from_counts(bits, contexts)

    # Passo 3: codificação rANS
    t2 = time.time()
    bitstream, final_state = encode_bits_contextual(bits, contexts, model)
    t3 = time.time()

    # Cabeçalhos
    if mode == "static":
        headers_hex = model.serialize_static_headers()
    else:
        headers_hex = model.serialize_adaptive_header()

    # Métricas
    bits_bs     = len(bitstream)
    bits_state  = final_state.bit_length() if final_state > 0 else 1
    bits_hdr    = model.header_overhead_bits()
    bits_total  = bits_bs + bits_state + bits_hdr
    orig_bits   = n_pixels * 8  # tamanho real sem padding
    taxa        = (1.0 - bits_total / orig_bits) * 100 if orig_bits > 0 else 0.0

    print(f"Codificação em {t3 - t2:.3f}s")
    print(f"  Bits originais (8 bits/pixel): {orig_bits}")
    print(f"  Bits bitstream rANS:           {bits_bs}")
    print(f"  Bits estado final:             {bits_state}")
    print(f"  Overhead cabeçalho:            {bits_hdr}")
    print(f"  Total transmitido:             {bits_total}")
    print(f"  Taxa de compressão:            {taxa:.2f}%")

    # Salva arquivo comprimido (binário para eficiência)
    # Formato: [4b H][4b W][4b n_bits_bitstream][4b final_state]
    #          [4b len_hdr_hex][hdr_hex bytes][bitstream bytes]
    hdr_bytes = headers_hex.encode("ascii")
    bs_bits   = list(map(int, bitstream))
    # Empacota bitstream em bytes
    padded = bs_bits[:]
    while len(padded) % 8:
        padded.append(0)
    bs_bytes = bytes(int("".join(map(str, padded[j:j+8])), 2)
                     for j in range(0, len(padded), 8))

    with open(output_path, "wb") as f:
        f.write(struct.pack(">IIIIII",
                            H, W, len(bitstream), final_state,
                            len(hdr_bytes), n_pixels))
        f.write(hdr_bytes)
        f.write(bs_bytes)
        # Escreve mode e recalc_window como texto terminado em \n
        f.write(f"{mode}\n{recalc_window}\n".encode("ascii"))

    print(f"Arquivo salvo: '{output_path}'")
    return {"orig_bits": orig_bits, "total_bits": bits_total,
            "taxa": taxa, "H": H, "W": W}


def decode_image(encoded_path: str, output_path: str) -> bool:
    """
    Decodifica imagem comprimida por encode_image.
    Retorna True se saída foi gravada com sucesso.
    """
    if not _CV2_AVAILABLE:
        raise ImportError("OpenCV (cv2) é necessário. Instale com: pip install opencv-python")

    with open(encoded_path, "rb") as f:
        H, W, n_bs_bits, final_state, n_hdr, n_pixels = struct.unpack(">IIIIII", f.read(24))
        hdr_bytes = f.read(n_hdr)
        n_bs_bytes = (n_bs_bits + 7) // 8
        bs_bytes   = f.read(n_bs_bytes)
        meta       = f.read().decode("ascii")

    headers_hex = hdr_bytes.decode("ascii")
    lines = meta.strip().split("\n")
    mode          = lines[0].strip() if len(lines) > 0 else "static"
    recalc_window = int(lines[1].strip()) if len(lines) > 1 else 1000

    # Desempacota bitstream
    bs_bits = []
    for byte in bs_bytes:
        for b in range(7, -1, -1):
            bs_bits.append((byte >> b) & 1)
    bitstream_str = "".join(map(str, bs_bits[:n_bs_bits]))

    bits_per_pixel = 9
    n_bits_total = n_pixels * bits_per_pixel

    # Modelo — idêntico ao encoder
    model = ContextModel(mode=mode, num_contexts=NUM_CONTEXTS_2D,
                         recalc_window=recalc_window)
    if mode == "static":
        model.load_static_headers(headers_hex)
    else:
        model.load_adaptive_header(headers_hex)

    bits_per_pixel = 9

    def pos_ctx(i: int) -> int:
        return min(i % bits_per_pixel, 3)

    from collections import deque
    bits_q       = deque(map(int, bitstream_str[::-1]))
    decoded_bits = [0] * n_bits_total
    state        = final_state

    for i in range(n_bits_total - 1, -1, -1):
        ctx    = pos_ctx(i)
        counts = model.get_counts(ctx)
        M      = counts[0] + counts[1]
        slot   = state % M
        s      = 1 if slot >= counts[0] else 0
        cs     = [0, counts[0]]
        prev   = (state // M) * counts[s] + slot - cs[s]

        while prev < M and bits_q:
            prev = (prev << 1) | bits_q.popleft()

        state           = prev
        decoded_bits[i] = s
        model.advance_window_decoder()

    # Reconstrói resíduos e imagem
    residuals = bits_to_residuals(decoded_bits, (H, W))
    img_out   = reconstruct_image(residuals)

    cv2.imwrite(output_path, img_out)
    print(f"Imagem reconstruída salva em '{output_path}' ({W}×{H} px)")
    return True



# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Spatial Predictor MED + rANS para imagem")
    parser.add_argument("--encode",        default="",            help="Caminho da imagem a codificar")
    parser.add_argument("--decode",        default="",            help="Arquivo comprimido a decodificar")
    parser.add_argument("--output",        default="",            help="Caminho de saída")
    parser.add_argument("--mode",          default="static",      choices=["static", "adaptive"])
    parser.add_argument("--recalc-window", type=int, default=1000)
    parser.add_argument("--verify",        action="store_true",   help="Verifica lossless após decode")
    args = parser.parse_args()

    if args.encode:
        out = args.output or (os.path.splitext(args.encode)[0] + "_encoded.bin")
        metrics = encode_image(args.encode, out, args.mode, args.recalc_window)

        if args.decode or args.verify:
            dec_out = os.path.splitext(args.encode)[0] + "_decoded.png"
            decode_image(out, dec_out)

            if args.verify:
                if not _CV2_AVAILABLE:
                    print("cv2 não disponível para verificação.")
                    return
                orig = cv2.imread(args.encode, cv2.IMREAD_GRAYSCALE)
                rec  = cv2.imread(dec_out,     cv2.IMREAD_GRAYSCALE)
                if orig is not None and rec is not None and np.array_equal(orig, rec):
                    print("\n✓  LOSSLESS: imagem reconstruída é 100% idêntica à original!")
                else:
                    print("\n✗  ERRO: imagem reconstruída difere da original!")

    elif args.decode:
        out = args.output or (os.path.splitext(args.decode)[0] + "_decoded.png")
        decode_image(args.decode, out)

    else:
        # Demo padrão: encode + decode + verify foto.jpg
        if not os.path.exists("foto.jpg"):
            print("Arquivo 'foto.jpg' não encontrado. Use --encode <imagem>.")
            return
        for mode in ["static", "adaptive"]:
            print(f"\n{'='*60}")
            print(f"  Modo: {mode}")
            print("=" * 60)
            out_enc = f"foto_encoded_{mode}.bin"
            out_dec = f"foto_decoded_{mode}.png"
            encode_image("foto.jpg", out_enc, mode=mode, recalc_window=1000)
            decode_image(out_enc, out_dec)
            if _CV2_AVAILABLE:
                orig = cv2.imread("foto.jpg", cv2.IMREAD_GRAYSCALE)
                rec  = cv2.imread(out_dec,    cv2.IMREAD_GRAYSCALE)
                if orig is not None and rec is not None and np.array_equal(orig, rec):
                    print("✓  LOSSLESS verificado!")
                else:
                    print("✗  Verificação falhou!")


if __name__ == "__main__":
    main()
