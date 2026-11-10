import math
import numpy as np
d = np.loadtxt("../dados.txt")
def br(x, c): return f"{x:,.{c}f}".replace(",", "X").replace(".", ",").replace("X", ".")
for col, h, nome in [(0, 8, "estatura"), (1, 4, "peso")]:
    v = d[:, col]; li = math.floor(v.min() / h) * h; k = int((v.max() - li) // h) + 1
    f = np.bincount(np.minimum(((v - li) / h).astype(int), k - 1), minlength=k)
    x = li + (np.arange(k) + 0.5) * h
    mu = (x * f).sum() / f.sum(); q = f * (x - mu) ** 2
    with open(f"tabela_{nome}.tex", "w") as o:
        for i in range(k):
            a = li + i * h
            o.write(f"{i+1} & ${a:.0f} \\vdash {a+h:.0f}$ & {br(x[i],0)} & {f[i]} & {br(x[i]*f[i],0)} & {br(q[i],2)} \\\\\n")
        o.write(f"\\midrule\n & \\textbf{{Total}} & & \\textbf{{{f.sum()}}} & \\textbf{{{br((x*f).sum(),0)}}} & \\textbf{{{br(q.sum(),2)}}} \\\\\n")
        o.write("\\bottomrule\n")
    print(nome, k, mu, math.sqrt(q.sum()/f.sum()), math.sqrt(q.sum()/f.sum())/mu*100, v.mean(), v.std(), v.std()/v.mean()*100, (x*f).sum(), q.sum())
