import cv2
import numpy as np
import matplotlib.pyplot as plt

def bit_plane_slicing(image_path):
    img = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)

    if img is None:
        print("Erro ao carregar a imagem.")
        return

    planes_visual = []
    planes_binary = {}

    for i in range(8):
        plane_mask = np.full(img.shape, 2 ** i, np.uint8)
        res = cv2.bitwise_and(img, plane_mask)

        binary_matrix = (res > 0).astype(np.uint8)

        if i in [5, 6, 7]:
            planes_binary[i] = binary_matrix

        planes_visual.append(binary_matrix * 255)

    for bit in [7, 6, 5]:
        filename = f'plano_bit_{bit}.txt'
        # Flatten para garantir que salve como sequência contínua de 0s e 1s
        np.savetxt(filename, planes_binary[bit].flatten(), fmt='%d', delimiter='')
        print(f"Plano {bit} salvo com sucesso em: {filename}")

    plt.figure(figsize=(12, 8))
    for i in range(8):
        plt.subplot(2, 4, i + 1)
        plt.imshow(planes_visual[i], cmap='gray')
        plt.title(f'Plano de Bit {i}')
        plt.axis('off')

    plt.tight_layout()
    plt.savefig('resultado_planos_bits.png', dpi=300)
    print("Gráfico dos planos de bits salvo com sucesso como 'resultado_planos_bits.png'!")
    plt.show()  # Bug fix: exibe o gráfico na tela além de salvar o arquivo


if __name__ == "__main__":
    bit_plane_slicing("foto.jpg")
