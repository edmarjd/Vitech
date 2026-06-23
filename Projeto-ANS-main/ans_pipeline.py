import os
import importlib.util
import numpy as np
from Bit_Planes import BitPlaneSlicer

def import_ans_encoder():
    """
    Imports the ANS-Streaming Encoder module using importlib due to the space in the filename.
    """
    module_name = "ans_streaming_encoder"
    file_path = "ANS-Streaming Encoder.py"
    spec = importlib.util.spec_from_file_location(module_name, file_path)
    if spec is None:
        raise ImportError(f"Could not find {file_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

def main():
    print("=========================================================================")
    print("       INTEGRATED PIPELINE: BIT PLANE SLICING -> ANS ENCODER")
    print("=========================================================================")

    IMAGE_PATH = 'foto.jpg'
    PLANES_TO_PROCESS = [7, 6, 5]
    BLOCK_SIZE = 16  # Matching the default in ANS-Streaming Encoder.py
    M_FACTOR = 0.99

    if not os.path.exists(IMAGE_PATH):
        print(f"Error: Image {IMAGE_PATH} not found.")
        return

    # 1. Initialize BitPlaneSlicer
    try:
        slicer = BitPlaneSlicer(IMAGE_PATH)
    except Exception as e:
        print(f"Slicer Error: {e}")
        return

    # 2. Generate visualization image
    slicer.visualize_planes('resultado_planos_bits.png')

    # 3. Import ANS Encoder logic
    try:
        ans_encoder = import_ans_encoder()
    except Exception as e:
        print(f"Encoder Import Error: {e}")
        return

    # 4. Process each plane
    for plane_idx in PLANES_TO_PROCESS:
        print(f"\n--- Processing Plane {plane_idx} ---")
        
        # Extract plane as numpy array (0s and 1s)
        binary_data = slicer.extract_plane(plane_idx)
        
        # Save as continuous binary sequence in .txt as required
        slicer.save_plane_to_txt(plane_idx, f'plano_bit_{plane_idx}.txt')
        
        # Convert numpy array to list of ints for the encoder
        symbols = binary_data.tolist()
        num_symbols = len(symbols)
        print(f"Total symbols: {num_symbols}")

        # Split into blocks (with padding)
        blocks = [symbols[i : i + BLOCK_SIZE] for i in range(0, num_symbols, BLOCK_SIZE)]
        if len(blocks[-1]) < BLOCK_SIZE:
            padding = BLOCK_SIZE - len(blocks[-1])
            blocks[-1].extend([0] * padding)
            print(f"Padded last block with {padding} zeros.")

        global_bitstream = []
        total_compressed_bits = 0
        
        # Compress blocks using the encoder logic
        for block in blocks:
            B = len(block)
            M = int(np.floor(M_FACTOR * B))
            
            c0 = block.count(0)
            c1 = block.count(1)
            
            # Use encoder's frequency scaling
            symbol_counts = ans_encoder.scale_frequencies(c0, c1, M, B)
            
            # Encode block
            final_state, block_bitstream = ans_encoder.Streaming_rANS_encoder_block(block, symbol_counts)
            
            global_bitstream.append(block_bitstream)
            total_compressed_bits += len(block_bitstream) + final_state.bit_length()

        # Save compressed result
        output_bin = f'compressed_plane_{plane_idx}.bin'
        with open(output_bin, 'w') as f:
            f.write(''.join(global_bitstream))
        
        original_bits = num_symbols
        compression_ratio = ((original_bits - total_compressed_bits) / original_bits) * 100
        print(f"Compression complete. Output: {output_bin}")
        print(f"Original: {original_bits} bits | Compressed: {total_compressed_bits} bits")
        print(f"Ratio: {compression_ratio:.2f}%")

    print("\n=========================================================================")
    print("Pipeline execution finished successfully.")
    print("=========================================================================")

if __name__ == "__main__":
    main()
