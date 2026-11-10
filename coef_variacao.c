#define _POSIX_C_SOURCE 199309L
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#define AMPLITUDE_ESTATURA 8.0
#define AMPLITUDE_PESO 4.0
#define PI 3.14159265358979323846

typedef struct {
    const char *nome;
    const char *unidade;
    double amplitude;
    double minimo;
    double maximo;
    double limite_inferior;
    int k;
    long *freq;
    long n;
    double media;
    double desvio;
    double cv;
    double media_bruta;
    double desvio_bruto;
    double cv_bruto;
} Distribuicao;

static double agora(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

static int num_threads(void)
{
#ifdef _OPENMP
    return omp_get_max_threads();
#else
    return 1;
#endif
}

static void *alocar(void *ptr, size_t bytes)
{
    void *novo = realloc(ptr, bytes);
    if (!novo) {
        fprintf(stderr, "Erro: memoria insuficiente\n");
        exit(EXIT_FAILURE);
    }
    return novo;
}

static int linha_em_branco(const char *linha)
{
    return linha[strspn(linha, " \t\r\n")] == '\0';
}

static int interpretar_linha(char *linha, double *a, double *b)
{
    int virgula_separa = strchr(linha, '.') != NULL;
    char extra;

    for (char *c = linha; *c; c++) {
        if (*c == ';' || *c == '\t') *c = ' ';
        else if (*c == ',') *c = virgula_separa ? ' ' : '.';
    }

    if (sscanf(linha, "%lf %lf %c", a, b, &extra) != 2) return 0;
    return isfinite(*a) && isfinite(*b) && *a > 0.0 && *b > 0.0;
}

static long ler_dados(const char *caminho, double **estatura, double **peso)
{
    FILE *f = fopen(caminho, "r");
    if (!f) {
        perror(caminho);
        exit(EXIT_FAILURE);
    }

    size_t capacidade = 1024;
    long n = 0, numero_linha = 0, invalidas = 0, primeira_invalida = 0;
    int cabecalho = 0;
    double *e = alocar(NULL, capacidade * sizeof *e);
    double *p = alocar(NULL, capacidade * sizeof *p);
    char linha[512];
    double a, b;

    while (fgets(linha, sizeof linha, f)) {
        numero_linha++;
        if (linha_em_branco(linha)) continue;

        if (!interpretar_linha(linha, &a, &b)) {
            if (n == 0 && invalidas == 0 && !cabecalho) {
                cabecalho = 1;
            } else {
                if (invalidas == 0) primeira_invalida = numero_linha;
                invalidas++;
            }
            continue;
        }

        if ((size_t)n == capacidade) {
            capacidade *= 2;
            e = alocar(e, capacidade * sizeof *e);
            p = alocar(p, capacidade * sizeof *p);
        }
        e[n] = a;
        p[n] = b;
        n++;
    }
    fclose(f);

    if (cabecalho)
        fprintf(stderr, "Aviso: primeira linha nao numerica tratada como cabecalho e ignorada\n");
    if (invalidas > 0)
        fprintf(stderr, "Aviso: %ld linha(s) invalida(s) ignorada(s); a primeira e a linha %ld\n",
                invalidas, primeira_invalida);
    if (n == 0) {
        fprintf(stderr, "Erro: nenhum par 'estatura peso' valido em %s\n", caminho);
        exit(EXIT_FAILURE);
    }

    *estatura = e;
    *peso = p;
    return n;
}

static uint64_t splitmix64(uint64_t x)
{
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

static double uniforme(uint64_t semente)
{
    return ((double)(splitmix64(semente) >> 11) + 0.5) * (1.0 / 9007199254740992.0);
}

static double normal(uint64_t semente)
{
    double u1 = uniforme(semente * 2);
    double u2 = uniforme(semente * 2 + 1);
    return sqrt(-2.0 * log(u1)) * cos(2.0 * PI * u2);
}

static void gerar_sinteticos(long n, double **estatura, double **peso)
{
    double *e = alocar(NULL, (size_t)n * sizeof *e);
    double *p = alocar(NULL, (size_t)n * sizeof *p);

    #pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++) {
        uint64_t s = (uint64_t)i * 3;
        double h = 168.0 + 9.0 * normal(s);
        double imc = 25.5 + 4.2 * normal(s + 1);
        h = fmin(fmax(h, 140.0), 205.0);
        imc = fmin(fmax(imc, 16.0), 42.0);
        e[i] = round(h * 10.0) / 10.0;
        p[i] = round(imc * (h / 100.0) * (h / 100.0) * 10.0) / 10.0;
    }

    *estatura = e;
    *peso = p;
}

static void calcular_extremos(const double *v, long n, double *minimo, double *maximo)
{
    double mn = v[0], mx = v[0];

    #pragma omp parallel for reduction(min:mn) reduction(max:mx) schedule(static)
    for (long i = 0; i < n; i++) {
        if (v[i] < mn) mn = v[i];
        if (v[i] > mx) mx = v[i];
    }

    *minimo = mn;
    *maximo = mx;
}

static void montar_classes(Distribuicao *d)
{
    d->limite_inferior = floor(d->minimo / d->amplitude) * d->amplitude;
    double classes = floor((d->maximo - d->limite_inferior) / d->amplitude) + 1.0;
    if (classes > 100000.0) {
        fprintf(stderr, "Erro: %s vai de %.1f a %.1f %s (%.0f classes); verifique o arquivo de entrada\n",
                d->nome, d->minimo, d->maximo, d->unidade, classes);
        exit(EXIT_FAILURE);
    }
    d->k = (int)classes;
    d->freq = calloc((size_t)d->k, sizeof *d->freq);
    if (!d->freq) {
        fprintf(stderr, "Erro: memoria insuficiente\n");
        exit(EXIT_FAILURE);
    }
}

static void contar_frequencias(Distribuicao *d, const double *v, long n)
{
    long *freq = d->freq;
    const int k = d->k;
    const double li = d->limite_inferior;
    const double h = d->amplitude;

    #pragma omp parallel for reduction(+:freq[:k]) schedule(static)
    for (long i = 0; i < n; i++) {
        int classe = (int)((v[i] - li) / h);
        if (classe >= k) classe = k - 1;
        freq[classe]++;
    }
}

static double ponto_medio(const Distribuicao *d, int i)
{
    return d->limite_inferior + (i + 0.5) * d->amplitude;
}

static void calcular_media_agrupada(Distribuicao *d)
{
    double soma_xf = 0.0;
    long soma_f = 0;

    #pragma omp parallel for reduction(+:soma_xf, soma_f) schedule(static)
    for (int i = 0; i < d->k; i++) {
        soma_xf += ponto_medio(d, i) * d->freq[i];
        soma_f += d->freq[i];
    }

    d->n = soma_f;
    d->media = soma_xf / soma_f;
}

static void calcular_desvio_agrupado(Distribuicao *d)
{
    const double mu = d->media;
    double soma = 0.0;

    #pragma omp parallel for reduction(+:soma) schedule(static)
    for (int i = 0; i < d->k; i++) {
        double desvio = ponto_medio(d, i) - mu;
        soma += d->freq[i] * desvio * desvio;
    }

    d->desvio = sqrt(soma / d->n);
    d->cv = d->desvio / d->media * 100.0;
}

static void calcular_estatisticas_brutas(Distribuicao *d, const double *v, long n)
{
    double soma = 0.0;

    #pragma omp parallel for reduction(+:soma) schedule(static)
    for (long i = 0; i < n; i++)
        soma += v[i];

    const double mu = soma / n;
    double soma_quad = 0.0;

    #pragma omp parallel for reduction(+:soma_quad) schedule(static)
    for (long i = 0; i < n; i++) {
        double desvio = v[i] - mu;
        soma_quad += desvio * desvio;
    }

    d->media_bruta = mu;
    d->desvio_bruto = sqrt(soma_quad / n);
    d->cv_bruto = d->desvio_bruto / mu * 100.0;
}

static void analisar(Distribuicao *d, const double *v, long n)
{
    calcular_extremos(v, n, &d->minimo, &d->maximo);
    montar_classes(d);
    contar_frequencias(d, v, n);
    calcular_media_agrupada(d);
    calcular_desvio_agrupado(d);
    calcular_estatisticas_brutas(d, v, n);
}

static void liberar(Distribuicao *d)
{
    free(d->freq);
    d->freq = NULL;
}

static void imprimir_tabela(const Distribuicao *d)
{
    printf("\n=== %s (%s) | amplitude de classe = %.0f %s | k = %d classes ===\n",
           d->nome, d->unidade, d->amplitude, d->unidade, d->k);
    printf("%-18s %10s %8s %14s %18s\n", "Classe", "xi", "fi", "xi*fi", "fi*(xi-mu)^2");

    double total_xf = 0.0, total_quad = 0.0;
    for (int i = 0; i < d->k; i++) {
        double li = d->limite_inferior + i * d->amplitude;
        double xi = ponto_medio(d, i);
        double xf = xi * d->freq[i];
        double quad = d->freq[i] * (xi - d->media) * (xi - d->media);
        char rotulo[32];
        snprintf(rotulo, sizeof rotulo, "[%.0f, %.0f)", li, li + d->amplitude);
        printf("%-18s %10.1f %8ld %14.1f %18.2f\n", rotulo, xi, d->freq[i], xf, quad);
        total_xf += xf;
        total_quad += quad;
    }
    printf("%-18s %10s %8ld %14.1f %18.2f\n", "Total", "", d->n, total_xf, total_quad);

    printf("\nMedia ponderada (mu)         = %.4f %s\n", d->media, d->unidade);
    printf("Desvio padrao populacional   = %.4f %s\n", d->desvio, d->unidade);
    printf("Coeficiente de variacao (CV) = %.4f %%\n", d->cv);
    printf("[conferencia, dados brutos]  media = %.4f | desvio = %.4f | CV = %.4f %%\n",
           d->media_bruta, d->desvio_bruto, d->cv_bruto);
}

static int comparar_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static void uso(const char *prog)
{
    fprintf(stderr,
            "Uso:\n"
            "  %s <arquivo> [repeticoes]              analisa o arquivo (estatura peso por linha)\n"
            "  %s --sintetico <N> [repeticoes]        gera N pessoas em memoria (benchmark)\n"
            "  acrescente --csv para imprimir apenas: versao,n,threads,mediana_ms,min_ms\n",
            prog, prog);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    int csv = 0;
    int argc_util = 0;
    char *args[8];

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--csv") == 0) csv = 1;
        else if (argc_util < 8) args[argc_util++] = argv[i];
    }
    if (argc_util < 1) uso(argv[0]);

    double *estatura, *peso;
    long n;
    int repeticoes;
    const char *origem;
    double t_leitura = 0.0;

    if (strcmp(args[0], "--sintetico") == 0) {
        if (argc_util < 2) uso(argv[0]);
        n = atol(args[1]);
        repeticoes = argc_util > 2 ? atoi(args[2]) : 10;
        gerar_sinteticos(n, &estatura, &peso);
        origem = "dados sinteticos em memoria";
    } else {
        repeticoes = argc_util > 1 ? atoi(args[1]) : 1000;
        double t0 = agora();
        n = ler_dados(args[0], &estatura, &peso);
        t_leitura = agora() - t0;
        origem = args[0];
    }

    if (n <= 0 || repeticoes <= 0) uso(argv[0]);

    Distribuicao est = {.nome = "Estatura", .unidade = "cm", .amplitude = AMPLITUDE_ESTATURA};
    Distribuicao pes = {.nome = "Peso", .unidade = "kg", .amplitude = AMPLITUDE_PESO};

    double *tempos = alocar(NULL, (size_t)repeticoes * sizeof *tempos);
    for (int r = 0; r < repeticoes; r++) {
        liberar(&est);
        liberar(&pes);
        double t0 = agora();
        analisar(&est, estatura, n);
        analisar(&pes, peso, n);
        tempos[r] = agora() - t0;
    }

    qsort(tempos, (size_t)repeticoes, sizeof *tempos, comparar_double);
    double mediana = tempos[repeticoes / 2] * 1e3;
    double minimo = tempos[0] * 1e3;

#ifdef _OPENMP
    const char *versao = "paralela";
#else
    const char *versao = "sequencial";
#endif

    if (csv) {
        printf("%s,%ld,%d,%.6f,%.6f\n", versao, n, num_threads(), mediana, minimo);
    } else {
        printf("Versao: %s | threads: %d | origem: %s | N = %ld\n", versao, num_threads(), origem, n);
        imprimir_tabela(&est);
        imprimir_tabela(&pes);
        printf("\n=== Resultado ===\n");
        printf("CV da estatura = %.2f %%\n", est.cv);
        printf("CV do peso     = %.2f %%\n", pes.cv);
        printf("\n=== Tempo ===\n");
        if (t_leitura > 0.0) printf("Leitura do arquivo (fora da medicao): %.3f ms\n", t_leitura * 1e3);
        printf("Calculo (%d repeticoes): mediana = %.6f ms | minimo = %.6f ms\n", repeticoes, mediana, minimo);
    }

    liberar(&est);
    liberar(&pes);
    free(tempos);
    free(estatura);
    free(peso);
    return EXIT_SUCCESS;
}
