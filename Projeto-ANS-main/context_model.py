"""
context_model.py
----------------
Modelo de contexto configurável de ordem 1 para compressão rANS binária.

Mudanças introduzidas neste módulo (ref. diagnóstico):
- Ponto 2/3: implementa contexto de ordem 1 (bit anterior/próximo) nos modos
  'static' (tabela fixa por janela RECALC_WINDOW, com cabeçalho compacto de
  12 bits por contexto, igual ao FSE/zstd) e 'adaptive' (estatísticas globais
  do input inteiro, zero overhead de cabeçalho extra por janela).
- Ponto 1: estado ANS NÃO é controlado aqui; este módulo só gerencia
  probabilidades. O controle de estado fica no encoder/decoder.

Contexto 1D  (para sequências INPUT_*): 2 contextos — prev=0 e prev=1.
Contexto 2D  (para imagem, em spatial_predictor.py): 4 contextos —
             (bit_esq, bit_acima) = 00, 01, 10, 11.

Nota sobre compatibilidade com rANS de decodificação reversa
------------------------------------------------------------
O rANS padrão decodifica na ordem REVERSA dos símbolos. Para que o contexto
seja causal e simétrico entre encoder e decoder, usamos "lookahead context":
    contexto do símbolo i  =  símbolo[i+1]  (0 se i == último)
No encoder (loop forward, i=0..n-1):  ctx = symbols[i+1] se i<n-1 else 0
No decoder (loop reverso, i=n-1..0): ctx = último_decodificado  (= symbol[i+1])
Isso garante que encoder e decoder usem EXATAMENTE a mesma tabela em cada passo.
"""

# ---------------------------------------------------------------------------
# Constantes
# ---------------------------------------------------------------------------

NUM_CONTEXTS_1D = 2   # 0 e 1 (bit anterior/próximo)
NUM_CONTEXTS_2D = 4   # 00, 01, 10, 11

PRECISION_BITS = 12           # bits de precisão para normalização FSE/zstd
PRECISION      = 1 << PRECISION_BITS  # 4096 — soma total das freqs normalizadas


# ---------------------------------------------------------------------------
# Classe principal
# ---------------------------------------------------------------------------

class ContextModel:
    """
    Gerencia probabilidades condicionadas ao contexto para o rANS binário.

    Parâmetros
    ----------
    mode : 'static' | 'adaptive'
        'static'   — tabela fixa por janela; recalculada a cada RECALC_WINDOW
                     símbolos; cada nova tabela é transmitida como cabeçalho
                     compacto (12 bits × num_contexts por janela).
        'adaptive' — estatísticas globais do input calculadas em dois passos
                     (encoder faz varredura prévia); decoder recebe o modelo
                     como único cabeçalho no início do stream (24 bits para
                     2 contextos), sem overhead por janela.
    num_contexts : int
        2 para sequências 1D, 4 para imagens 2D.
    recalc_window : int
        Tamanho da janela de recalibração (apenas mode='static').
    """

    def __init__(self, mode: str = "static", num_contexts: int = NUM_CONTEXTS_1D,
                 recalc_window: int = 1000):
        if mode not in ("static", "adaptive"):
            raise ValueError(f"mode deve ser 'static' ou 'adaptive', recebeu '{mode}'")
        self.mode          = mode
        self.num_contexts  = num_contexts
        self.recalc_window = recalc_window

        # Prior de Laplace: c0=1, c1=1 por contexto
        # Em adaptive: substituído pelas estatísticas globais antes de codificar
        self._freq = [[1, 1] for _ in range(num_contexts)]

        # Contagens da janela atual (modo static)
        self._win_counts = [[1, 1] for _ in range(num_contexts)]

        # Contador de símbolos (controla troca de janela no modo static)
        self._sym_count = 0

        # Lista de headers (modo static): cada item = lista de c0_norm por contexto
        self._headers: list[list[int]] = []

    # ------------------------------------------------------------------
    # API do encoder
    # ------------------------------------------------------------------

    def set_global_frequencies(self, symbols: list[int]) -> None:
        """
        (Modo adaptive) Calcula frequências globais por contexto a partir de
        uma varredura completa dos símbolos (com lookahead context).
        Deve ser chamado pelo encoder ANTES de iniciar a codificação.
        """
        counts = [[1, 1] for _ in range(self.num_contexts)]
        n = len(symbols)
        for i in range(n):
            ctx = symbols[i + 1] if i + 1 < n else 0
            counts[ctx][symbols[i]] += 1
        # Normaliza para PRECISION
        self._freq = self._normalize(counts)

    def prime_static_from_scan(self, symbols: list) -> None:
        """
        (Modo static) Inicializa a tabela com varredura prévia de todos os
        símbolos. Usa uma única tabela global para toda a codificação,
        SEM recalibração intermediária.

        Isso resolve a incompatibilidade de janelas com decodificação reversa:
        o encoder trocaria de tabela na posição i (esquerda→direita), mas o
        decoder reverso contaria da direita — as trocas nunca coincidiriam.
        Com _no_recalc=True, apenas o header inicial é transmitido
        (overhead fixo = num_contexts × 12 bits, independente do stream).
        """
        counts = [[1, 1] for _ in range(self.num_contexts)]
        n = len(symbols)
        for i in range(n):
            ctx = symbols[i + 1] if i + 1 < n else 0
            counts[ctx][symbols[i]] += 1
        self._freq = self._normalize(counts)
        self._init_header = [c[0] for c in self._freq]
        self._no_recalc = True  # desativa recalibração intermediária

    def prime_static_from_counts(self, bits: list, contexts: list) -> None:
        """
        (Modo static, imagem) Inicializa com bits+contexts pré-calculados.
        Equivalente a prime_static_from_scan mas aceita ctx separado.
        """
        counts = [[1, 1] for _ in range(self.num_contexts)]
        for b, ctx in zip(bits, contexts):
            counts[ctx][b] += 1
        self._freq = self._normalize(counts)
        self._init_header = [c[0] for c in self._freq]
        self._no_recalc = True


    def get_counts(self, ctx: int) -> list[int]:
        """Retorna [c0, c1] para uso direto no C_rANS / D_rANS."""
        return self._freq[ctx]

    def update_after_symbol(self, ctx: int, sym: int) -> bool:
        """
        Atualiza o modelo após processar 'sym' no contexto 'ctx'.
        Se _no_recalc=True (modo static com priming), ignora recalibração
        intermediária — tabela permanece fixa durante todo o stream.
        """
        new_window = False
        if self.mode == "static" and not getattr(self, "_no_recalc", False):
            self._win_counts[ctx][sym] += 1
            self._sym_count += 1
            if self._sym_count % self.recalc_window == 0:
                new_table = self._normalize(self._win_counts)
                self._headers.append([c[0] for c in new_table])
                self._freq = new_table
                self._win_counts = [[1, 1] for _ in range(self.num_contexts)]
                new_window = True
        return new_window

    # ------------------------------------------------------------------
    # Serialização (encoder → arquivo) e Deserialização (arquivo → decoder)
    # ------------------------------------------------------------------

    def serialize_static_headers(self) -> str:
        """
        Serializa todos os headers de janelas (modo static) como string hex.
        O primeiro header é SEMPRE a tabela inicial (da varredura prévia ou
        do prior). Demais headers = janelas subsequentes.
        Cada header = num_contexts × 12 bits de c0_norm.
        Overhead total: (1 + num_janelas_disparadas) × num_contexts × 12 bits.
        """
        # Inclui o header inicial como primeiro elemento
        init = getattr(self, "_init_header", None)
        all_headers = ([init] if init else []) + self._headers
        if not all_headers:
            return ""
        bits: list[int] = []
        for hdr in all_headers:
            for c0n in hdr:
                for b in range(PRECISION_BITS - 1, -1, -1):
                    bits.append((c0n >> b) & 1)
        while len(bits) % 8:
            bits.append(0)
        val = int("".join(map(str, bits)), 2)
        return format(val, f"0{len(bits) // 4}x")

    def load_static_headers(self, hex_str: str) -> None:
        """
        Carrega headers serializados pelo encoder (modo static).
        O PRIMEIRO header é aplicado imediatamente como tabela inicial.
        Os demais são aplicados a cada RECALC_WINDOW símbolos.
        """
        if not hex_str:
            self._headers = []
            return
        n_bits = len(hex_str) * 4
        val = int(hex_str, 16)
        bits = [(val >> (n_bits - 1 - i)) & 1 for i in range(n_bits)]
        bph = self.num_contexts * PRECISION_BITS  # bits per header
        self._headers = []
        pos = 0
        while pos + bph <= len(bits):
            hdr = []
            for _ in range(self.num_contexts):
                c0n = 0
                for _ in range(PRECISION_BITS):
                    c0n = (c0n << 1) | bits[pos]
                    pos += 1
                hdr.append(c0n)
            if any(v > 0 for v in hdr):
                self._headers.append(hdr)
        # Aplica imediatamente o primeiro header como tabela ativa
        if self._headers:
            self._apply_header(self._headers[0])
        # Se só há 1 header, o encoder usou tabela única (prime mode) — decoder
        # também deve usar tabela fixa sem avançar janelas.
        if len(self._headers) == 1:
            self._no_recalc = True

    def serialize_adaptive_header(self) -> str:
        """
        (Modo adaptive) Serializa o modelo global como único cabeçalho hex.
        = serialize_static_headers com um único header sintético.
        """
        hdr = [max(1, min(PRECISION - 1, self._freq[c][0]))
               for c in range(self.num_contexts)]
        self._headers = [hdr]
        return self.serialize_static_headers()

    def load_adaptive_header(self, hex_str: str) -> None:
        """
        (Modo adaptive) Carrega e aplica o modelo global do cabeçalho.
        """
        self.load_static_headers(hex_str)
        if self._headers:
            self._apply_header(self._headers[0])

    def advance_window_decoder(self) -> None:
        """
        (Modo static) Chamado pelo decoder após cada símbolo.
        Se _no_recalc=True, a tabela é fixa — não avança nada.
        """
        if self.mode != "static" or getattr(self, "_no_recalc", False):
            return
        self._sym_count += 1
        if self._sym_count % self.recalc_window == 0:
            next_idx = self._sym_count // self.recalc_window
            if next_idx < len(self._headers):
                self._apply_header_idx(next_idx)

    def header_overhead_bits(self) -> int:
        """Total de bits de overhead dos cabeçalhos (para relatório)."""
        if self.mode == "static":
            if getattr(self, "_no_recalc", False):
                # Apenas o header inicial único
                return self.num_contexts * PRECISION_BITS
            # +1 pelo header inicial + headers de janelas disparadas
            n_init = 1 if getattr(self, "_init_header", None) else 0
            return (n_init + len(self._headers)) * self.num_contexts * PRECISION_BITS
        # adaptive: único header global
        return self.num_contexts * PRECISION_BITS

    # ------------------------------------------------------------------
    # Utilitários internos
    # ------------------------------------------------------------------

    def _normalize(self, counts: list[list[int]]) -> list[list[int]]:
        """Normaliza contagens para soma = PRECISION; garante c0, c1 >= 1."""
        result = []
        for c0, c1 in counts:
            total = c0 + c1
            c0n = round(c0 * PRECISION / total)
            c0n = max(1, min(PRECISION - 1, c0n))
            result.append([c0n, PRECISION - c0n])
        return result

    def _apply_header(self, hdr: list[int]) -> None:
        self._freq = []
        for c0n in hdr:
            c0n = max(1, min(PRECISION - 1, c0n))
            self._freq.append([c0n, PRECISION - c0n])

    def _apply_header_idx(self, idx: int) -> None:
        if 0 <= idx < len(self._headers):
            self._apply_header(self._headers[idx])
