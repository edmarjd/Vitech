import random

def generate_biased_test(filename, p1=0.7, n=100):
    symbols = []
    for _ in range(n):
        if random.random() < p1:
            symbols.append(1)
        else:
            symbols.append(0)
    
    with open(filename, 'w') as f:
        # Escrever 10 símbolos por linha para facilitar a leitura
        for i in range(0, n, 10):
            line = ' '.join(map(str, symbols[i:i+10]))
            f.write(line + '\n')
    
    print(f"Gerado arquivo {filename} com {n} símbolos (P(1)={p1})")

if __name__ == "__main__":
    generate_biased_test('teste_entropy.txt', p1=0.8, n=100)
