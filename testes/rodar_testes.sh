#!/usr/bin/env bash
set -uo pipefail

cd "$(dirname "$0")/.."
TESTES=testes
CASOS=$TESTES/casos
falhas=0
total=0

verifica() {
    total=$((total + 1))
    if [ "$1" = "0" ]; then
        echo "  [OK]     $2"
    else
        echo "  [FALHOU] $2"
        falhas=$((falhas + 1))
    fi
}

sem_tempo() {
    grep -v -E "Versao|Leitura|Calculo"
}

echo "== 1. Compilacao estrita =="
make -s cv_seq cv_par
gcc -O2 -std=c11 -Wall -Wextra -Wpedantic -Werror -Wno-unknown-pragmas -o /tmp/cv_estrito_seq coef_variacao.c -lm
verifica $? "versao sequencial sem avisos (-Wall -Wextra -Wpedantic -Werror)"
gcc -O2 -std=c11 -Wall -Wextra -Wpedantic -Werror -fopenmp -o /tmp/cv_estrito_par coef_variacao.c -lm
verifica $? "versao paralela sem avisos (-Wall -Wextra -Wpedantic -Werror)"

echo "== 2. Conferencia independente (aritmetica exata em Python) =="
python3 -I $TESTES/conferir.py dados.txt ./cv_seq > /dev/null
verifica $? "dados.txt: limites, classes, fi, media, desvio e CV"
python3 -I $TESTES/conferir.py $CASOS/mao.txt ./cv_seq > /dev/null
verifica $? "exemplo calculado a mao (mao.txt)"
./cv_seq $CASOS/mao.txt 1 | grep -q "Coeficiente de variacao (CV) = 3.6262 %"
verifica $? "mao.txt: CV da estatura = 3,6262 % (calculado a mao)"
./cv_seq $CASOS/mao.txt 1 | grep -q "Coeficiente de variacao (CV) = 7.9253 %"
verifica $? "mao.txt: CV do peso = 7,9253 % (calculado a mao)"

echo "== 3. Sequencial x paralela =="
./cv_seq dados.txt 1 | sem_tempo > /tmp/cv_ref.txt
for t in 1 2 3 4 6 8 12; do
    OMP_NUM_THREADS=$t ./cv_par dados.txt 1 | sem_tempo | diff -q /tmp/cv_ref.txt - > /dev/null
    verifica $? "dados.txt com $t thread(s): saida identica a sequencial"
done
./cv_seq --sintetico 2000000 1 | sem_tempo > /tmp/cv_ref_g.txt
OMP_NUM_THREADS=8 ./cv_par --sintetico 2000000 1 | sem_tempo | diff -q /tmp/cv_ref_g.txt - > /dev/null
verifica $? "2 milhoes de pessoas com 8 threads: saida identica a sequencial"

echo "== 4. Casos de borda =="
espera_cv() {
    OMP_NUM_THREADS=4 ./cv_par "$CASOS/$1" 1 2>/dev/null | grep -q "CV da estatura = $2 %"
    verifica $? "$1 -> CV estatura $2 % $3"
}
espera_erro() {
    OMP_NUM_THREADS=4 ./cv_par "$CASOS/$1" 1 > /dev/null 2>/tmp/cv_err.txt
    [ $? -ne 0 ] && grep -q "$2" /tmp/cv_err.txt
    verifica $? "$1 -> encerra com erro claro ($2)"
}
espera_aviso() {
    OMP_NUM_THREADS=4 ./cv_par "$CASOS/$1" 1 2>&1 >/dev/null | grep -q "$2"
    verifica $? "$1 -> avisa: $2"
}
espera_cv uma.txt 0.00 "(uma pessoa)"
espera_cv iguais.txt 0.00 "(todos iguais)"
espera_cv cabecalho.txt 2.38 "(cabecalho ignorado)"
espera_aviso cabecalho.txt "cabecalho"
espera_cv crlf.txt 2.38 "(quebra de linha do Windows)"
espera_cv tabs.txt 2.38 "(tabulacao e linha em branco)"
espera_cv virgula.txt 5.89 "(virgula decimal)"
./cv_seq $CASOS/virgula.txt 1 | grep -q "Media ponderada (mu)         = 169.3333 cm"
verifica $? "virgula.txt -> media agrupada calculada a mao (169,3333 cm)"
espera_cv pontovirgula.txt 4.88 "(separador ;)"
espera_cv csv.txt 4.88 "(separador , com decimal .)"
espera_aviso lixo.txt "1 linha(s) invalida(s)"
espera_aviso negativo.txt "2 linha(s) invalida(s)"
espera_aviso trescolunas.txt "1 linha(s) invalida(s)"
espera_erro vazio.txt "nenhum par"
espera_erro outlier.txt "verifique o arquivo"
./cv_par $CASOS/nao_existe.txt > /dev/null 2>&1
[ $? -ne 0 ]
verifica $? "arquivo inexistente -> encerra com erro"

echo "== 5. Memoria e comportamento indefinido (AddressSanitizer + UBSan) =="
gcc -g -O1 -std=c11 -fopenmp -fsanitize=address,undefined -fno-omit-frame-pointer -o /tmp/cv_asan coef_variacao.c -lm
asan_ok=0
for f in dados.txt $CASOS/*.txt; do
    OMP_NUM_THREADS=4 ASAN_OPTIONS=detect_leaks=1 /tmp/cv_asan "$f" 2 > /tmp/cv_asan.txt 2>&1
    grep -q -E "Sanitizer|runtime error" /tmp/cv_asan.txt && { asan_ok=1; echo "    problema em $f"; }
done
verifica $asan_ok "nenhum acesso invalido, vazamento ou comportamento indefinido"

echo "== 6. Condicao de corrida (ThreadSanitizer + Archer, se disponivel) =="
ARCHER=$(ls /usr/lib/llvm-*/lib/libarcher.so 2>/dev/null | head -1)
if command -v clang > /dev/null && [ -n "$ARCHER" ] && \
   clang -g -O1 -std=c11 -fopenmp -fsanitize=thread -o /tmp/cv_tsan coef_variacao.c -lm 2>/dev/null; then
    OMP_NUM_THREADS=4 OMP_TOOL_LIBRARIES=$ARCHER TSAN_OPTIONS="ignore_noninstrumented_modules=1" \
        /tmp/cv_tsan --sintetico 50000 2 > /tmp/cv_tsan.txt 2>&1
    ! grep -q "WARNING: ThreadSanitizer" /tmp/cv_tsan.txt
    verifica $? "nenhuma condicao de corrida detectada"
else
    echo "  [PULADO] precisa de clang, libomp-dev e libclang-rt-dev (sudo apt install clang libomp-dev libclang-rt-18-dev)"
fi

echo
echo "Resultado: $((total - falhas)) de $total testes passaram"
[ $falhas -eq 0 ]
