# Desafio final OpenMP: coeficiente de variação com dados agrupados

Este programa calcula o coeficiente de variação (CV) da estatura e do peso de 1000 pessoas. A estatura é agrupada em classes de 8 cm e o peso em classes de 4 kg. Os laços de cálculo são paralelizados com OpenMP.

## Arquivos

| Arquivo | Conteúdo |
|---|---|
| `coef_variacao.c` | Código-fonte. Gera a versão sequencial e a paralela a partir do mesmo fonte. |
| `Makefile` | Compila `cv_seq` (sem `-fopenmp`) e `cv_par` (com `-fopenmp`). |
| `dados.txt` | 1000 linhas no formato `estatura peso`. |
| `gerar_dados.py` | Gera a base de dados de forma reprodutível (semente 2026). |
| `benchmark.sh` | Mede os tempos e gera `resultados.csv`. |
| `testes/` | `rodar_testes.sh` (32 verificações automáticas), `conferir.py` (conferência com aritmética exata) e arquivos de casos de borda. |
| `relatorio.pdf` | Relatório com a paralelização, os resultados e a análise de desempenho. |
| `relatorio/` | Fonte LaTeX do relatório e scripts dos gráficos e tabelas. |

## Uso

```bash
make
./cv_seq dados.txt
OMP_NUM_THREADS=4 ./cv_par dados.txt
./cv_par --sintetico 10000000 10
THREADS="1 2 4 6 12" ./benchmark.sh
make test
```

## Resultados

| Variável | Média | Desvio padrão | CV |
|---|---|---|---|
| Estatura | 167,752 cm | 9,5232 cm | **5,68 %** |
| Peso | 72,348 kg | 14,1582 kg | **19,57 %** |

## Formato de entrada

O arquivo deve ter uma pessoa por linha, com a estatura e o peso. O programa aceita ponto ou vírgula decimal e separadores espaço, tabulação, `;` ou `,`. Também aceita uma linha de cabeçalho e linhas em branco. Linhas inválidas são ignoradas, e o programa avisa quantas foram descartadas.
