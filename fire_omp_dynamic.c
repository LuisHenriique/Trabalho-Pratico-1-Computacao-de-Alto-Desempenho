#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <omp.h>

#define IDX(l, c, max_c) (((long long)(l)) * (max_c) + (c))

enum Cobertura {
    AGUA = 0,
    SOLO_EXPOSTO = 1,
    VEGETACAO_RASTEIRA = 2,
    FLORESTA = 3
};

enum Estado {
    NAO_COMBUSTIVEL = 0,
    INTACTA = 1,
    EM_CHAMAS = 2,
    QUEIMADA = 3,
    CONTENCAO = 4
};

typedef struct {
    int linha;
    int coluna;
} PosicaoFogo;

typedef struct {
    int passo;
    int linha_ini, coluna_ini;
    int linha_fim, coluna_fim;
} ZonaContencao;

typedef struct {
    int intensidade;
    int dir_linha;
    int dir_coluna;
} Vento;

typedef struct {
    int L, C;

    unsigned char *cobertura;
    unsigned char *umidade;
    int *ativacao;

    unsigned char *estado;
    unsigned char *proximo_estado;

    unsigned char *tempo_atual;
    unsigned char *proximo_tempo;
} Terreno;

typedef struct {
    int max_passos;
    int threads;
    int limiar;
    unsigned int seed;
    int n_focos;
    int n_contencoes;
    Vento vento;
} Configuracao;

typedef struct {
    int passos;
    int combustiveis;
    int ignicoes;
    int passo_mais_ignicoes;
    int max_ignicoes;
    double tempo_exec;
} Estatisticas;

static const int fator_combustivel[] = {
    [AGUA] = 0,
    [SOLO_EXPOSTO] = 0,
    [VEGETACAO_RASTEIRA] = 8,
    [FLORESTA] = 12
};

static const unsigned char tempo_inicial[] = {
    [VEGETACAO_RASTEIRA] = 2,
    [FLORESTA] = 4
};

static const unsigned char estado_inicial[] = {
    [AGUA] = NAO_COMBUSTIVEL,
    [SOLO_EXPOSTO] = NAO_COMBUSTIVEL,
    [VEGETACAO_RASTEIRA] = INTACTA,
    [FLORESTA] = INTACTA
};

/* ==========================================================================
 * Declarações das Funções
 * ========================================================================== */
static inline unsigned char gerar_cobertura(int index);
void erro(const char *str);

void alocar_terreno(Terreno *t, int L, int C);
void liberar_terreno(Terreno *t);
void ler_entrada(FILE *fd, Configuracao *c, Terreno *t, PosicaoFogo **focos, ZonaContencao **contencoes);
void validar_entrada(Configuracao *c, Terreno *t, PosicaoFogo *focos, ZonaContencao *contencoes);
void validar_focos_iniciais(Terreno *t, PosicaoFogo *focos, int n_focos);
void gerar_terreno(Terreno *t, unsigned *seed, long long n_cells);
void iniciar_fogos(Terreno *t, PosicaoFogo *focos, int n_focos);
void construir_contencao(Terreno *t, ZonaContencao *contencoes, int n_contencoes);
int potencial_ignicao(Terreno *t, int linha, int coluna, Vento v);
int contar_combustiveis(Terreno *t, long long n_cells);
void processar_celula(Terreno *t, Configuracao *c, long long i, int passo, int *ignicoes_passo, int *pegando_fogo);
Terreno preparar_mundo(char *arquivo_entrada, Configuracao *c, Estatisticas *s);
int verificar_fogo(unsigned char *celular, int n_cells);
void simular(Configuracao *c, Estatisticas *s, Terreno *t);
void imprimir_resultados(Terreno *t, Estatisticas *s, long long n_cells);

/* ==========================================================================
 * Função Principal
 * ========================================================================== */
int main(int argc, char *argv[])
{
    if(argc != 2)
        erro("Uso: ./fire_seq nome_arquivo_entrada.txt");

    Configuracao config;
    Estatisticas stats = {0};

    Terreno mapa = preparar_mundo(argv[1], &config, &stats);

    simular(&config, &stats, &mapa);

    imprimir_resultados(&mapa, &stats, (long long)mapa.C * mapa.L);

    liberar_terreno(&mapa);

    return 0;
}

/* ==========================================================================
 * Implementações
 * ========================================================================== */
void erro(const char *str)
{
    fprintf(stderr,"%s\n", str);
    exit(1);
}

static inline unsigned char gerar_cobertura(int index) {
    if (index < 10)  return AGUA;
    if (index < 20)  return SOLO_EXPOSTO;
    if (index < 55)  return VEGETACAO_RASTEIRA;
    if (index < 100) return FLORESTA;
    return 0;
}

void alocar_terreno(Terreno *t, int L, int C)
{
    size_t n_cells = (size_t)L * C;

    t->cobertura = malloc(n_cells * sizeof(unsigned char));
    t->umidade = malloc(n_cells * sizeof(unsigned char));
    t->estado = malloc(n_cells * sizeof(unsigned char));
    t->tempo_atual = calloc(n_cells, sizeof(unsigned char));
    t->ativacao = malloc(n_cells * sizeof(int));
    t->proximo_estado = malloc(n_cells * sizeof(unsigned char));
    t->proximo_tempo = malloc(n_cells * sizeof(unsigned char));

    assert(t->cobertura && t->umidade && t->estado && t->tempo_atual && t->ativacao && t->proximo_estado && t->proximo_tempo);
}

void liberar_terreno(Terreno *t)
{
    free(t->cobertura);
    free(t->umidade);
    free(t->estado);
    free(t->tempo_atual);
    free(t->ativacao);
    free(t->proximo_estado);
    free(t->proximo_tempo);
}

void ler_entrada(FILE *fd, Configuracao *c, Terreno *t, PosicaoFogo **focos, ZonaContencao **contencoes)
{
    fscanf(fd, "%d %d %d %d %u %d", &t->L, &t->C, &c->max_passos, &c->threads, &c->seed, &c->limiar);
    fscanf(fd, "%d %d %d", &c->vento.dir_linha, &c->vento.dir_coluna, &c->vento.intensidade);
    fscanf(fd, "%d %d", &c->n_focos, &c->n_contencoes);

    if(c->n_focos < 0 || c->n_contencoes < 0)
        erro("Valor invalido na entrada!");

    *focos = malloc(c->n_focos * sizeof(PosicaoFogo));
    assert(*focos);
    *contencoes = malloc(c->n_contencoes * sizeof(ZonaContencao));
    assert(*contencoes);

    for(int i = 0; i < c->n_focos; i++)
        fscanf(fd, "%d %d", &(*focos)[i].linha, &(*focos)[i].coluna);
    for(int i = 0; i < c->n_contencoes; i++) {
        ZonaContencao *z = &(*contencoes)[i];
        fscanf(fd, "%d %d %d %d %d", &z->passo, &z->linha_ini, &z->coluna_ini, &z->linha_fim, &z->coluna_fim);
    }
}

void validar_entrada(Configuracao *c, Terreno *t, PosicaoFogo *focos, ZonaContencao *contencoes)
{
    if(t->L <= 0 || t->C <= 0 || c->max_passos < 0 || c->threads <= 0 || c->limiar <= 0)
        erro("Parametros gerais invalidos.");

    if(c->n_focos < 0 || c->n_contencoes < 0)
        erro("Quantidades negativas de focos ou contencoes.");

    if(c->vento.dir_linha < -1 || c->vento.dir_linha > 1) erro("Direcao linha do vento.");
    if(c->vento.dir_coluna < -1 || c->vento.dir_coluna > 1) erro("Direcao coluna do vento.");
    if(c->vento.dir_linha == 0 && c->vento.dir_coluna == 0) erro("Vento nulo (0,0).");
    if(c->vento.intensidade < 0 || c->vento.intensidade > 5) erro("Intensidade do vento.");

    ZonaContencao *z;
    for(int i = 0; i < c->n_contencoes; i++) {
        z = &contencoes[i];

        if(z->linha_ini < 0 || z->linha_ini >= t->L) erro("Linha inicial da contencao invalida.");
        if(z->coluna_ini < 0 || z->coluna_ini >= t->C) erro("Coluna inicial da contencao invalida.");
        if(z->linha_fim < 0 || z->linha_fim >= t->L) erro("Linha final da contencao invalida.");
        if(z->coluna_fim < 0 || z->coluna_fim >= t->C) erro("Coluna final da contencao invalida.");

        if(z->linha_ini > z->linha_fim || z->coluna_ini > z->coluna_fim)
            erro("Limites da contencao invertidos.");

        if(z->passo < 0 || z->passo >= c->max_passos)
            erro("Passo da contencao fora do limite.");
    }

    for(int i = 0; i < c->n_focos; i++) {
        if(focos[i].linha < 0 || focos[i].linha >= t->L) erro("Linha do foco invalida.");
        if(focos[i].coluna < 0 || focos[i].coluna >= t->C) erro("Coluna do foco invalida.");

        for(int j = i + 1; j < c->n_focos; j++) {
            if(focos[i].linha == focos[j].linha && focos[i].coluna == focos[j].coluna)
                erro("Focos repetidos.");
        }
    }
}

void validar_focos_iniciais(Terreno *t, PosicaoFogo *focos, int n_focos)
{
    long long idx;
    for(int i = 0; i < n_focos; i++) {
        idx = IDX(focos[i].linha, focos[i].coluna, t->C);
        if(t->cobertura[idx] == AGUA || t->cobertura[idx] == SOLO_EXPOSTO)
            erro("Foco inicial não pode estar em água ou solo exposto");
    }
}

void gerar_terreno(Terreno *t, unsigned *seed, long long n_cells)
{
    for(long long i = 0; i < n_cells; i++) {
        int rand_val = rand_r(seed) % 100;
        t->cobertura[i] = gerar_cobertura(rand_val);
        t->umidade[i] = rand_r(seed) % 101;
        t->estado[i] = estado_inicial[t->cobertura[i]];
    }
}

void iniciar_fogos(Terreno *t, PosicaoFogo *focos, int n_focos)
{
    long long j;
    for(int i = 0; i < n_focos; i++) {
        j = IDX(focos[i].linha, focos[i].coluna, t->C);
        t->estado[j] = EM_CHAMAS;
        t->tempo_atual[j] = tempo_inicial[t->cobertura[j]];
    }
}

void construir_contencao(Terreno *t, ZonaContencao *contencoes, int n_contencoes)
{
    memset(t->ativacao, -1, sizeof(int) * (size_t)t->C * t->L);

    ZonaContencao zona;
    long long l;
    for(int i = 0; i < n_contencoes; i++) {
        zona = contencoes[i];
        for(int j = zona.linha_ini; j <= zona.linha_fim; j++) {
            for(int k = zona.coluna_ini; k <= zona.coluna_fim; k++) {
                l = IDX(j, k, t->C);
                if(t->ativacao[l] == -1 || t->ativacao[l] > zona.passo)
                    t->ativacao[l] = zona.passo;
            }
        }
    }
}

int potencial_ignicao(Terreno *t, int linha, int coluna, Vento v)
{
    int peso_total = 0;

    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            int l_viz, c_viz;
            int prop_l, prop_c;
            int peso_basico, alinhamento;

            if (dr == 0 && dc == 0) continue;

            l_viz = linha + dr;
            c_viz = coluna + dc;

            if (l_viz < 0 || l_viz >= t->L || c_viz < 0 || c_viz >= t->C) continue;

            if (t->estado[IDX(l_viz, c_viz, t->C)] == EM_CHAMAS) {
                prop_l = -dr;
                prop_c = -dc;

                peso_basico = (abs(prop_l) + abs(prop_c) == 1) ? 10 : 7;
                alinhamento = (prop_l * v.dir_linha) + (prop_c * v.dir_coluna);

                int peso = peso_basico + (v.intensidade * alinhamento);
                peso_total += (peso < 1) ? 1 : peso;
            }
        }
    }

    long long idx = IDX(linha, coluna, t->C);
    return (peso_total * fator_combustivel[t->cobertura[idx]] * (100 - t->umidade[idx])) / 100;
}

int contar_combustiveis(Terreno *t, long long n_cells)
{
    int combustiveis = 0;
    for(long long i = 0; i < n_cells; i++)
        if(t->cobertura[i] == VEGETACAO_RASTEIRA || t->cobertura[i] == FLORESTA)
            combustiveis++;
    return combustiveis;
}

Terreno preparar_mundo(char *arquivo_entrada, Configuracao *c, Estatisticas *s)
{
    FILE *fd;
    Terreno t;
    PosicaoFogo *focos;
    ZonaContencao *contencoes;

    s->passo_mais_ignicoes = -1;

    if ((fd = fopen(arquivo_entrada, "r")) == NULL)
        erro("Arquivo não encontrado");

    ler_entrada(fd, c, &t, &focos, &contencoes);
    fclose(fd);

    validar_entrada(c, &t, focos, contencoes);

    alocar_terreno(&t, t.L, t.C);

    gerar_terreno(&t, &c->seed, (long long)t.L * t.C);
    validar_focos_iniciais(&t, focos, c->n_focos);

    iniciar_fogos(&t, focos, c->n_focos);
    free(focos);

    construir_contencao(&t, contencoes, c->n_contencoes);
    free(contencoes);

    s->combustiveis = contar_combustiveis(&t, (long long)t.C * t.L);

    return t;
}

void processar_celula(Terreno *t, Configuracao *c, long long i, int passo, int *ignicoes_passo, int *pegando_fogo)
{
    t->proximo_estado[i] = t->estado[i];
    t->proximo_tempo[i] = t->tempo_atual[i];

    if (t->ativacao[i] == passo && t->estado[i] == INTACTA) {
        t->proximo_estado[i] = CONTENCAO;
        return;
    }

    if (t->estado[i] == INTACTA && potencial_ignicao(t, i / t->C, i % t->C, c->vento) >= c->limiar) {
        t->proximo_estado[i] = EM_CHAMAS;
        t->proximo_tempo[i] = tempo_inicial[t->cobertura[i]];

        *pegando_fogo = 1;
        (*ignicoes_passo)++;
    } else if (t->estado[i] == EM_CHAMAS) {
        if (t->tempo_atual[i] == 1) {
            t->proximo_estado[i] = QUEIMADA;
        } else {
            *pegando_fogo = 1;
        }
        t->proximo_tempo[i]--;
    }
}

/* Verdadeiro se existe pelo menos uma celula em chamas */
int verificar_fogo(unsigned char *estado, int n_cells)
{
	int tem_fogo = 0;
    for (long long i = 0; i < n_cells; i++) {
        if (estado[i] == EM_CHAMAS) {
			return 1;
        }
    }
	return 0;
}

void simular(Configuracao *c, Estatisticas *s, Terreno *t)
{
    long long n_cells = (long long)t->C * t->L;
    double inicio = omp_get_wtime();

    
    if (!verificar_fogo(t->estado, n_cells)) {
        s->passos = 0;
        s->tempo_exec = omp_get_wtime() - inicio;
        return;
    }

    int passo = 0;
	int pegando_fogo = 0;
	int ignicoes_passo = 0;
	int continuar = 1;
	#pragma omp parallel num_threads(c->threads) default(none) shared(continuar, passo, n_cells, t, c, s, pegando_fogo, ignicoes_passo)
	while(passo < c->max_passos && continuar) {
		#pragma omp for simd schedule(dynamic, 512) reduction(+:ignicoes_passo) reduction(max:pegando_fogo)
        for(long long i = 0; i < n_cells; i++) {
            processar_celula(t, c, i, passo, &ignicoes_passo, &pegando_fogo);
        }

		#pragma omp single
		{
			s->ignicoes += ignicoes_passo;
			if (ignicoes_passo > s->max_ignicoes) {
				s->max_ignicoes = ignicoes_passo;
				s->passo_mais_ignicoes = passo;
			}

			unsigned char *temp_est = t->estado;
			t->estado = t->proximo_estado;
			t->proximo_estado = temp_est;

			unsigned char *temp_tmp = t->tempo_atual;
			t->tempo_atual = t->proximo_tempo;
			t->proximo_tempo = temp_tmp;

			passo++;

			if (!pegando_fogo)
				continuar = 0;
			else {
				pegando_fogo = 0;
				ignicoes_passo = 0;
			}
		}
    }

    s->passos = passo;
    s->tempo_exec = omp_get_wtime() - inicio;
}

void imprimir_resultados(Terreno *t, Estatisticas *s, long long n_cells)
{
    int intactos = 0, em_chamas = 0, queimados = 0, contidos = 0, nao_combustiveis = 0;

    for (long long i = 0; i < n_cells; i++) {
        switch (t->estado[i]) {
            case NAO_COMBUSTIVEL:  nao_combustiveis++;  break;
            case INTACTA:          intactos++;          break;
            case EM_CHAMAS:        em_chamas++;         break;
            case QUEIMADA:         queimados++;         break;
            case CONTENCAO:        contidos++;          break;
        }
    }

    double queimados_porc = 0.0, protegidos_porc = 0.0;
    if(s->combustiveis > 0) {
        queimados_porc = 100.0 * (queimados + em_chamas) / s->combustiveis;
        protegidos_porc = 100.0 * contidos / s->combustiveis;
    }

    unsigned long long checksum = 0;
    for (long long i = 0; i < n_cells; i++) {
        checksum = checksum * 31ULL + (unsigned long long) t->estado[i];
        checksum = checksum * 31ULL + (unsigned long long) t->tempo_atual[i];
    }

    printf("passos: %d\n", s->passos);
    printf("nao_combustiveis: %d\n", nao_combustiveis);
    printf("intactas: %d\n", intactos);
    printf("em_chamas: %d\n", em_chamas);
    printf("queimadas: %d\n", queimados);
    printf("contencao: %d\n", contidos);
    printf("total_ignicoes: %d\n", s->ignicoes);
    printf("pico_ignicoes: %d %d\n", s->passo_mais_ignicoes, s->max_ignicoes);
    printf("percentual_queimado: %.2f\n", queimados_porc);
    printf("percentual_protegido: %.2f\n", protegidos_porc);
    printf("checksum: %llu\n", checksum);
    printf("tempo: %.6f\n", s->tempo_exec);
}
