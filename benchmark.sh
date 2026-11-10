#!/usr/bin/env bash
set -euo pipefail

THREADS="${THREADS:-1 2 4}"
TAMANHOS="${TAMANHOS:-10000 100000 1000000 10000000 20000000}"
SAIDA="${SAIDA:-resultados.csv}"

reps_para() {
    local n=$1
    if   (( n <= 10000 ));   then echo 2000
    elif (( n <= 100000 ));  then echo 300
    elif (( n <= 1000000 )); then echo 50
    else echo 10
    fi
}

echo "versao,n,threads,mediana_ms,min_ms" > "$SAIDA"

./cv_seq dados.txt 5000 --csv >> "$SAIDA"
for t in $THREADS; do
    OMP_NUM_THREADS=$t OMP_PROC_BIND=close ./cv_par dados.txt 5000 --csv >> "$SAIDA"
done

for n in $TAMANHOS; do
    r=$(reps_para "$n")
    ./cv_seq --sintetico "$n" "$r" --csv >> "$SAIDA"
    for t in $THREADS; do
        OMP_NUM_THREADS=$t OMP_PROC_BIND=close ./cv_par --sintetico "$n" "$r" --csv >> "$SAIDA"
    done
done

sed "s/,/\t/g" "$SAIDA"
