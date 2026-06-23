import cv2
import numpy as np
import matplotlib.pyplot as plt
import os

class BitPlaneSlicer:
    """
    Responsável pela extração de planos de bits de uma imagem em tons de cinza.
    """
    def __init__(self, image_path):
        self.image_path = image_path
        self.img = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
        if self.img is None:
            raise FileNotFoundError(f"Erro ao carregar a imagem em {image_path}")

    def extract_plane(self, plane_index):
        """
        Extrai um plano de bits específico (0-7) e retorna como um array binário linearizado.
        """
        if not (0 <= plane_index <= 7):
            raise ValueError("O índice do plano deve estar entre 0 e 7.")
        
        plane_mask = np.full(self.img.shape, 2 ** plane_index, np.uint8)
        res = cv2.bitwise_and(self.img, plane_mask)
        binary_matrix = (res > 0).astype(np.uint8)
        return binary_matrix.flatten()

    def save_plane_to_txt(self, plane_index, filename):
        """
        Extrai um plano e o salva em um arquivo de texto como uma sequência contínua de 0s e 1s.
        """
        plane_data = self.extract_plane(plane_index)
        # Salvando como uma string contínua de 0s e 1s (sem delimitador)
        np.savetxt(filename, plane_data, fmt='%d', delimiter='')
        print(f"Plano {plane_index} salvo com sucesso em: {filename}")

    def visualize_planes(self, output_path='resultado_planos_bits.png'):
        """
        Visualiza todos os 8 planos de bits usando matplotlib e salva o resultado em um arquivo.
        """
        planes_visual = []
        for i in range(8):
            plane_mask = np.full(self.img.shape, 2 ** i, np.uint8)
            res = cv2.bitwise_and(self.img, plane_mask)
            binary_matrix = (res > 0).astype(np.uint8)
            planes_visual.append(binary_matrix * 255)

        plt.figure(figsize=(12, 8))
        for i in range(8):
            plt.subplot(2, 4, i + 1)
            plt.imshow(planes_visual[i], cmap='gray')
            plt.title(f'Plano {i}')
            plt.axis('off')

        plt.tight_layout()
        plt.savefig(output_path)
        plt.close()
        print(f"Visualização salva em: {output_path}")

if __name__ == "__main__":
    # Exemplo de uso
    try:
        slicer = BitPlaneSlicer('foto.jpg')
        slicer.visualize_planes()
        # Extrair e salvar planos de maior significância
        for bit in [7, 6, 5]:
            slicer.save_plane_to_txt(bit, f'plano_bit_{bit}.txt')
    except Exception as e:
        print(f"Erro: {e}")
