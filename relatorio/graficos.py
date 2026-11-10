import csv, math
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

TXT, TXT2, GRID = "#1f1f1e", "#5f5e58", "#e6e5e0"
plt.rcParams.update({
    "font.family": "DejaVu Sans", "font.size": 9, "axes.edgecolor": GRID,
    "axes.labelcolor": TXT2, "xtick.color": TXT2, "ytick.color": TXT2,
    "axes.spines.top": False, "axes.spines.right": False, "axes.grid": True,
    "grid.color": GRID, "grid.linewidth": 0.6, "axes.axisbelow": True,
    "legend.frameon": False, "axes.titlesize": 10, "axes.titlecolor": TXT,
})
CORES = {"seq": "#2a78d6", 1: "#eb6834", 2: "#1baf7a", 4: "#eda100"}
MARC = {"seq": "o", 1: "s", 2: "^", 4: "D"}
ROT = {"seq": "Sequencial", 1: "Paralela, 1 thread", 2: "Paralela, 2 threads", 4: "Paralela, 4 threads"}

linhas = list(csv.DictReader(open("../resultados.csv")))
ns = sorted({int(l["n"]) for l in linhas})
def serie(chave):
    out = []
    for n in ns:
        for l in linhas:
            ok = l["versao"] == "sequencial" if chave == "seq" else (l["versao"] == "paralela" and int(l["threads"]) == chave)
            if ok and int(l["n"]) == n:
                out.append(float(l["mediana_ms"]))
    return np.array(out)

tempos = {k: serie(k) for k in ["seq", 1, 2, 4]}

fig, ax = plt.subplots(figsize=(6.2, 3.4))
for k, t in tempos.items():
    ax.plot(ns, t, color=CORES[k], marker=MARC[k], markersize=5, lw=2, label=ROT[k],
            markeredgecolor="white", markeredgewidth=0.8)
ax.set_xscale("log"); ax.set_yscale("log")
ax.set_xlabel("Número de pessoas (N)"); ax.set_ylabel("Tempo de cálculo (ms, mediana)")
ax.set_title("Tempo de execução por tamanho da entrada", loc="left")
ax.legend(loc="upper left", fontsize=8)
fig.tight_layout(); fig.savefig("fig_tempo.pdf")

fig, ax = plt.subplots(figsize=(6.2, 3.2))
ax.axhline(1, color=TXT2, lw=1, ls="--")
ax.text(ns[-1], 0.94, "speedup = 1 (sem ganho)", color=TXT2, fontsize=7.5, va="top", ha="right")
ax.axhline(2, color=TXT2, lw=1, ls=":")
ax.text(ns[0], 2.04, "ideal com 2 núcleos físicos (speedup = 2)", color=TXT2, fontsize=7.5, va="bottom")
for k in [1, 2, 4]:
    s = tempos["seq"] / tempos[k]
    ax.plot(ns, s, color=CORES[k], marker=MARC[k], markersize=5, lw=2, label=ROT[k],
            markeredgecolor="white", markeredgewidth=0.8)
ax.set_xscale("log"); ax.set_ylim(0, 2.4)
ax.set_xlabel("Número de pessoas (N)"); ax.set_ylabel("Speedup  (T_seq / T_par)")
ax.set_title("Speedup em relação à versão sequencial", loc="left")
ax.legend(loc="lower right", fontsize=8)
fig.tight_layout(); fig.savefig("fig_speedup.pdf")

d = np.loadtxt("../dados.txt")
fig, axs = plt.subplots(1, 2, figsize=(6.4, 2.6))
for ax, col, h, nome, un in [(axs[0], 0, 8, "Estatura", "cm"), (axs[1], 1, 4, "Peso", "kg")]:
    v = d[:, col]; li = math.floor(v.min() / h) * h; k = int((v.max() - li) // h) + 1
    f = np.bincount(np.minimum(((v - li) / h).astype(int), k - 1), minlength=k)
    bordas = li + np.arange(k) * h
    ax.bar(bordas, f, width=h, align="edge", color="#2a78d6", edgecolor="white", linewidth=1.5)
    ax.set_title(f"{nome} — classes de {h} {un}", loc="left")
    ax.set_xlabel(f"{nome} ({un})"); ax.set_ylabel("Frequência (fi)")
    ax.grid(axis="x", visible=False)
fig.tight_layout(); fig.savefig("fig_histogramas.pdf")

with open("tabela_desempenho.tex", "w") as out:
    for i, n in enumerate(ns):
        ts = tempos["seq"][i]
        cel = [f"{n:,}".replace(",", "."), f"{ts:.4f}".replace(".", ",")]
        for k in [2, 4]:
            tp = tempos[k][i]; s = ts / tp
            cel += [f"{tp:.4f}".replace(".", ","), f"{s:.2f}".replace(".", ","), f"{s / k * 100:.0f}\\%"]
        out.write(" & ".join(cel) + r" \\" + "\n")
    out.write("\\bottomrule\n")
print(open("tabela_desempenho.tex").read())
