import sys
import numpy as np

n = int(sys.argv[1]) if len(sys.argv) > 1 else 1000
saida = sys.argv[2] if len(sys.argv) > 2 else "dados.txt"
rng = np.random.default_rng(2026)

sexo = rng.random(n) < 0.5
estatura = np.where(sexo, rng.normal(175.0, 7.5, n), rng.normal(162.0, 6.8, n))
estatura = np.clip(estatura, 140.0, 205.0)
imc = np.clip(rng.normal(25.5, 4.2, n), 16.0, 42.0)
peso = imc * (estatura / 100.0) ** 2

with open(saida, "w") as f:
    for h, p in zip(estatura, peso):
        f.write(f"{h:.1f} {p:.1f}\n")
