import sys, re, subprocess
from fractions import Fraction as F
from decimal import Decimal, getcontext
getcontext().prec = 50

arq, binario = sys.argv[1], sys.argv[2]
linhas = [l.split() for l in open(arq) if l.strip()]
dados = {"Estatura": [F(Decimal(a)) for a, _ in linhas], "Peso": [F(Decimal(b)) for _, b in linhas]}
amp = {"Estatura": 8, "Peso": 4}

saida = subprocess.run([binario, arq, "1"], capture_output=True, text=True).stdout
blocos = re.split(r"\n=== ", saida)

def sqrt_exato(x):
    return Decimal(x.numerator) / Decimal(x.denominator)

ok = True
for nome, v in dados.items():
    h = amp[nome]
    mn = min(v)
    li = (mn // h) * h
    limites = [li]
    while limites[-1] <= max(v):
        limites.append(limites[-1] + h)
    k = len(limites) - 1
    f = [sum(1 for x in v if limites[i] <= x < limites[i + 1]) for i in range(k)]
    assert sum(f) == len(v)
    xs = [limites[i] + F(h, 2) for i in range(k)]
    N = sum(f)
    mu = sum(x * fi for x, fi in zip(xs, f)) / N
    var = sum(fi * (x - mu) ** 2 for x, fi in zip(xs, f)) / N
    sigma = sqrt_exato(var).sqrt()
    cv = sigma / sqrt_exato(mu) * 100

    bloco = next(b for b in blocos if b.startswith(nome))
    linhas_tab = re.findall(r"^\[(\d+), (\d+)\)\s+([\d.]+)\s+(\d+)", bloco, re.M)
    f_c = [int(l[3]) for l in linhas_tab]
    li_c = int(linhas_tab[0][0])
    mu_c = Decimal(re.search(r"Media ponderada \(mu\)\s+= ([\d.]+)", bloco).group(1))
    sg_c = Decimal(re.search(r"Desvio padrao populacional\s+= ([\d.]+)", bloco).group(1))
    cv_c = Decimal(re.search(r"Coeficiente de variacao \(CV\) = ([\d.]+)", bloco).group(1))

    checks = {
        "limite inferior": li_c == li,
        "k classes": len(f_c) == k,
        "frequencias fi": f_c == f,
        "media (4 casas)": abs(mu_c - sqrt_exato(mu)) <= Decimal("0.00005"),
        "desvio (4 casas)": abs(sg_c - sigma) <= Decimal("0.00005"),
        "CV (4 casas)": abs(cv_c - cv) <= Decimal("0.00005"),
    }
    print(f"{nome}: li={li} k={k} mu={sqrt_exato(mu):.6f} sigma={sigma:.6f} CV={cv:.6f}%")
    for c, r in checks.items():
        print(f"   {'OK ' if r else 'FALHOU'} {c}")
        ok &= r
print("RESULTADO GERAL:", "TUDO OK" if ok else "HA FALHAS")
sys.exit(0 if ok else 1)
