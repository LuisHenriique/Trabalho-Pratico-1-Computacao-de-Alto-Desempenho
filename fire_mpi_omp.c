#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <omp.h>
#include <mpi.h>

/* ==========================================================================
 * Declarações de Estruturas Auxiliares
 * ========================================================================== */

// Converte linha e coluna em uma posição do vetor
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

// Define quanto cada tipo de terreno ajuda o fogo a se espalhar
static const int fator_combustivel[] = {
    [AGUA] = 0,
    [SOLO_EXPOSTO] = 0,
    [VEGETACAO_RASTEIRA] = 8,
    [FLORESTA] = 12
};

// Define por quantos passos cada tipo de vegetação queima
static const unsigned char tempo_inicial[] = {
    [VEGETACAO_RASTEIRA] = 2,
    [FLORESTA] = 4
};

// Define o estado inicial de cada tipo de terreno
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

void alocar_linhas(int L, int C, int size, int *sendcounts, int *displs);
void aloca_arrays_locais(int n_linhas_p, int C, Terreno *local_mapa);
void copia_array_local_ui(int rank, int n_elementos, int *sendcounts, int *displs, unsigned char *array_global, unsigned char *array_local);

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
void processar_celula(Terreno *t, Configuracao *c, long long i,int C,  int passo, int *ignicoes_passo, int *pegando_fogo);
Terreno preparar_mundo(char *arquivo_entrada, Configuracao *c, Estatisticas *s);
int verificar_fogo(unsigned char *celular, int n_cells);
void simular(Configuracao *c, Estatisticas *s, Terreno *t, int rank, int size, int L_global);
void imprimir_resultados(Terreno *t, Estatisticas *s, long long n_cells);

/* ==========================================================================
 * Função Principal
 * ========================================================================== */
int main(int argc, char *argv[])
{
    Terreno mapa = {0};
    Configuracao config = {0};
    Estatisticas stats = {0};
    int provided, rank, size;
    int params[8];

    // Inicia o MPI e identifica o processo e o total de processos
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int *sendcounts = NULL, *displs = NULL;
    // O processo 0 lê a entrada e prepara a matriz completa
    if (rank == 0) {
        if (argc != 2) {
            fprintf(stderr, "Uso: mpirun -np <NPrc> ./fire_mpi_omp <arq_entrada>\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        mapa = preparar_mundo(argv[1], &config, &stats);

        params[0] = mapa.L;
        params[1] = mapa.C;
        params[2] = config.max_passos;
        params[3] = config.threads;
        params[4] = config.limiar;
        params[5] = config.vento.intensidade;
        params[6] = config.vento.dir_linha;
        params[7] = config.vento.dir_coluna;

        sendcounts = malloc(size * sizeof(int));
        displs = malloc(size * sizeof(int));
        alocar_linhas(mapa.L, mapa.C, size, sendcounts, displs);
    }

    // Envia os parâmetros da simulação para todos os processos
    MPI_Bcast(params, 8, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank != 0) {
        mapa.L = params[0];
        mapa.C = params[1];
        config.max_passos = params[2];
        config.threads = params[3];
        config.limiar = params[4];
        config.vento.intensidade = params[5];
        config.vento.dir_linha = params[6];
        config.vento.dir_coluna = params[7];
    }

    // Divide as linhas da matriz entre os processos
    int n_linhas_p = mapa.L / size + (rank < (mapa.L % size) ? 1 : 0);
    int elem_locais = n_linhas_p * mapa.C;

    Terreno mapa_local = {0};
    mapa_local.L = n_linhas_p;
    mapa_local.C = mapa.C;
    aloca_arrays_locais(n_linhas_p, mapa.C, &mapa_local);

    // Distribui os dados da matriz para as partes locais
    copia_array_local_ui(rank, elem_locais, sendcounts, displs, mapa.cobertura, mapa_local.cobertura);
    copia_array_local_ui(rank, elem_locais, sendcounts, displs, mapa.estado, &mapa_local.estado[mapa.C]);
    copia_array_local_ui(rank, elem_locais, sendcounts, displs, mapa.tempo_atual, mapa_local.tempo_atual);
    copia_array_local_ui(rank, elem_locais, sendcounts, displs, mapa.umidade, mapa_local.umidade);

    MPI_Scatterv(
        rank == 0 ? mapa.ativacao : NULL,
        sendcounts, displs, MPI_INT,
        mapa_local.ativacao, elem_locais, MPI_INT,
        0, MPI_COMM_WORLD
    );

    mapa_local.C = mapa.C;
    mapa_local.L = n_linhas_p;

    simular(&config, &stats, &mapa_local, rank, size, mapa.L);

    // Reúne os estados e tempos finais no processo 0
    MPI_Gatherv(
        &mapa_local.estado[mapa.C], elem_locais, MPI_UNSIGNED_CHAR,
        rank == 0 ? mapa.estado : NULL, sendcounts, displs, MPI_UNSIGNED_CHAR,
        0, MPI_COMM_WORLD
    );
    MPI_Gatherv(
        mapa_local.tempo_atual, elem_locais, MPI_UNSIGNED_CHAR,
        rank == 0 ? mapa.tempo_atual : NULL, sendcounts, displs, MPI_UNSIGNED_CHAR,
        0, MPI_COMM_WORLD
    );

    if (rank == 0) {
        imprimir_resultados(&mapa, &stats, (long long)mapa.C * mapa.L);
        liberar_terreno(&mapa);
        free(sendcounts);
        free(displs);
    }

    liberar_terreno(&mapa_local);
    MPI_Finalize();
    return 0;
}

/* ==========================================================================
 * Implementações
 * ========================================================================== */
void erro(const char *str)
{
    fprintf(stderr, "%s\n", str);
    MPI_Abort(MPI_COMM_WORLD, 1);
}

// Escolhe o tipo de terreno
static inline unsigned char gerar_cobertura(int index) {
    if (index < 10)  return AGUA;
    if (index < 20)  return SOLO_EXPOSTO;
    if (index < 55)  return VEGETACAO_RASTEIRA;
    if (index < 100) return FLORESTA;
    return 0;
}

// Calcula a quantidade de células e o início da faixa de cada processo
void alocar_linhas(int L, int C, int size, int *sendcounts, int *displs)
{
    int offset = 0;
    for (int r = 0; r < size; r++) {
        int linhas_alocadas = L / size + (r < (L % size) ? 1 : 0);
        sendcounts[r] = linhas_alocadas * C;
        displs[r] = offset;
        offset += sendcounts[r];
    }
}

// Reserva os vetores locais e as linhas extras de estado
void aloca_arrays_locais(int n_linhas_p, int C, Terreno *local_mapa)
{
    size_t n   = (size_t)n_linhas_p * C;
    size_t n_h = (size_t)(n_linhas_p + 2) * C;

    local_mapa->estado         = calloc(n_h, sizeof(unsigned char));
    local_mapa->proximo_estado = calloc(n_h, sizeof(unsigned char));
    local_mapa->ativacao       = malloc(n * sizeof(int) + 1);   // +1: evita malloc(0) -> NULL
    local_mapa->cobertura      = malloc(n + 1);
    local_mapa->umidade        = malloc(n + 1);
    local_mapa->tempo_atual    = malloc(n + 1);
    local_mapa->proximo_tempo  = malloc(n + 1);

    assert(local_mapa->estado && local_mapa->proximo_estado && local_mapa->ativacao &&
           local_mapa->cobertura && local_mapa->umidade &&
           local_mapa->tempo_atual && local_mapa->proximo_tempo);
}

// Distribui as partes de um vetor de bytes entre os processos
void copia_array_local_ui(int rank, int n_elementos, int *sendcounts, int *displs, unsigned char *array_global, unsigned char *array_local)
{
    MPI_Scatterv(
        rank == 0 ? array_global : NULL,
        sendcounts,
        displs,
        MPI_UNSIGNED_CHAR,
        array_local,
        n_elementos,
        MPI_UNSIGNED_CHAR,
        0,
        MPI_COMM_WORLD
    );
}

// Reserva os vetores usados pelo terreno a ser simulado
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

// Libera os vetores do terreno
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

// Lê as medidas, regras, focos e áreas de contenção do arquivo
void ler_entrada(FILE *fd, Configuracao *c, Terreno *t, PosicaoFogo **focos, ZonaContencao **contencoes)
{
    int lidos;
    lidos = fscanf(fd, "%d %d %d %d %u %d", &t->L, &t->C, &c->max_passos, &c->threads, &c->seed, &c->limiar);
    lidos = fscanf(fd, "%d %d %d", &c->vento.dir_linha, &c->vento.dir_coluna, &c->vento.intensidade);
    lidos = fscanf(fd, "%d %d", &c->n_focos, &c->n_contencoes);

    if(c->n_focos < 0 || c->n_contencoes < 0)
        erro("Valor invalido na entrada!");

    *focos = malloc(c->n_focos * sizeof(PosicaoFogo));
    assert(*focos);
    *contencoes = malloc(c->n_contencoes * sizeof(ZonaContencao));
    assert(*contencoes);

    for(int i = 0; i < c->n_focos; i++)
        lidos = fscanf(fd, "%d %d", &(*focos)[i].linha, &(*focos)[i].coluna);
    for(int i = 0; i < c->n_contencoes; i++) {
        ZonaContencao *z = &(*contencoes)[i];
        lidos = fscanf(fd, "%d %d %d %d %d", &z->passo, &z->linha_ini, &z->coluna_ini, &z->linha_fim, &z->coluna_fim);
    }
    (void)lidos;
}

// Confere os limites e evita posições inválidas ou repetidas
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

// Impede que um foco comece na água ou no solo exposto
void validar_focos_iniciais(Terreno *t, PosicaoFogo *focos, int n_focos)
{
    long long idx;
    for(int i = 0; i < n_focos; i++) {
        idx = IDX(focos[i].linha, focos[i].coluna, t->C);
        if(t->cobertura[idx] == AGUA || t->cobertura[idx] == SOLO_EXPOSTO)
            erro("Foco inicial não pode estar em água ou solo exposto");
    }
}

// Gera a cobertura, a umidade e o estado inicial de cada célula
void gerar_terreno(Terreno *t, unsigned *seed, long long n_cells)
{
    for(long long i = 0; i < n_cells; i++) {
        int rand_val = rand_r(seed) % 100;
        t->cobertura[i] = gerar_cobertura(rand_val);
        t->umidade[i] = rand_r(seed) % 101;
        t->estado[i] = estado_inicial[t->cobertura[i]];
    }
}

// Acende os focos iniciais e marca seu tempo de queima
void iniciar_fogos(Terreno *t, PosicaoFogo *focos, int n_focos)
{
    long long j;
    for(int i = 0; i < n_focos; i++) {
        j = IDX(focos[i].linha, focos[i].coluna, t->C);
        t->estado[j] = EM_CHAMAS;
        t->tempo_atual[j] = tempo_inicial[t->cobertura[j]];
    }
}

// Guarda o primeiro passo de contenção previsto para cada célula
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

// Soma a força do fogo vizinho, levando em conta vento e distância
int potencial_ignicao(Terreno *t, int linha, int coluna, Vento v)
{
    int peso_total = 0;

    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0) continue;

            int c_viz = coluna + dc;
            if (c_viz < 0 || c_viz >= t->C) continue;


            if (t->estado[IDX(linha + 1 + dr, c_viz, t->C)] == EM_CHAMAS) {
                int prop_l = -dr;
                int prop_c = -dc;

                int peso_basico = (abs(prop_l) + abs(prop_c) == 1) ? 10 : 7;
                int alinhamento = (prop_l * v.dir_linha) + (prop_c * v.dir_coluna);

                int peso = peso_basico + (v.intensidade * alinhamento);
                peso_total += (peso < 1) ? 1 : peso;
            }
        }
    }

    // Ajusta a força pelo tipo de vegetação e pela umidade
    long long idx = IDX(linha, coluna, t->C);
    return (peso_total * fator_combustivel[t->cobertura[idx]] * (100 - t->umidade[idx])) / 100;
}

// Conta as células com vegetação, usadas nos percentuais finais
int contar_combustiveis(Terreno *t, long long n_cells)
{
    int combustiveis = 0;
    for(long long i = 0; i < n_cells; i++)
        if(t->cobertura[i] == VEGETACAO_RASTEIRA || t->cobertura[i] == FLORESTA)
            combustiveis++;
    return combustiveis;
}

// Lê a entrada e monta o terreno antes da simulação
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

// Calcula o próximo estado da célula sem mudar o estado atual
void processar_celula(Terreno *t, Configuracao *c, long long i, int C, int passo, int *ignicoes_passo, int *pegando_fogo)
{
    t->proximo_estado[i+C] = t->estado[i+C];
    t->proximo_tempo[i] = t->tempo_atual[i];

    // A contenção protege a célula intacta antes da possível ignição
    if (t->ativacao[i] == passo && t->estado[i+C] == INTACTA) {
        t->proximo_estado[i+C] = CONTENCAO;
        return;
    }

    // Uma célula intacta pega fogo quando atinge o limiar
    if (t->estado[i+C] == INTACTA && potencial_ignicao(t, i / t->C, i % t->C, c->vento) >= c->limiar) {
        t->proximo_estado[i+C] = EM_CHAMAS;
        t->proximo_tempo[i] = tempo_inicial[t->cobertura[i]];

        *pegando_fogo = 1;
        (*ignicoes_passo)++;
    } else if (t->estado[i+C] == EM_CHAMAS) {
        // O fogo dura até o tempo de queima chegar a zero
        if (t->tempo_atual[i] == 1) {
            t->proximo_estado[i+C] = QUEIMADA;
        } else {
            *pegando_fogo = 1;
        }
        t->proximo_tempo[i]--;
    }
}

// Verdadeiro se existe pelo menos uma celula em chamas
int verificar_fogo(unsigned char *estado, int n_cells)
{
    for (long long i = 0; i < n_cells; i++) {
        if (estado[i] == EM_CHAMAS) {
			return 1;
        }
    }
	return 0;
}

// Troca as linhas de fronteira com os processos vizinhos
static inline void trocar_halo(unsigned char *est, int C, int L, int viz_cima, int viz_baixo)
{
    MPI_Sendrecv(&est[C],       C, MPI_UNSIGNED_CHAR, viz_cima,  0,
                 &est[(L+1)*C], C, MPI_UNSIGNED_CHAR, viz_baixo, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    MPI_Sendrecv(&est[L*C],     C, MPI_UNSIGNED_CHAR, viz_baixo, 1,
                 &est[0],       C, MPI_UNSIGNED_CHAR, viz_cima,  1,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

// Avança a simulação até acabar o fogo ou atingir o limite de passos
void simular(Configuracao *c, Estatisticas *s, Terreno *t, int rank, int size, int L_global)
{
    const int C = t->C, L = t->L;
    const long long n_cells = (long long)C * L;

    int viz_cima = MPI_PROC_NULL, viz_baixo = MPI_PROC_NULL;
    if (L > 0) {
        if (rank > 0)                         viz_cima  = rank - 1;
        if (rank < size - 1 && rank + 1 < L_global) viz_baixo = rank + 1;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double inicio = omp_get_wtime();

    int fogo_local  = (L > 0) ? verificar_fogo(&t->estado[C], n_cells) : 0;
    int fogo_global = 0;
    // Verifica se existe fogo em qualquer processo antes do primeiro passo
    MPI_Allreduce(&fogo_local, &fogo_global, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

    if (!fogo_global) {
        s->passos = 0;
        s->tempo_exec = omp_get_wtime() - inicio;
        return;
    }

    trocar_halo(t->estado, C, L, viz_cima, viz_baixo);

    int passo = 0;
    int pegando_fogo = 0;
    int ignicoes_passo = 0;
    int continuar = 1;

    // Mantém as threads ativas durante todos os passos
    #pragma omp parallel num_threads(c->threads) \
        shared(continuar, passo, n_cells, t, c, s, pegando_fogo, ignicoes_passo, C, L, viz_cima, viz_baixo)
    while (passo < c->max_passos && continuar) {

        // Processa as células locais e reduz as ignições e a presença de fogo
        #pragma omp for simd schedule(static) reduction(+:ignicoes_passo) reduction(max:pegando_fogo)
        for (long long i = 0; i < n_cells; i++)
            processar_celula(t, c, i, C, passo, &ignicoes_passo, &pegando_fogo);

        // A thread principal combina os resultados e atualiza os dados comuns
        #pragma omp master
        {
            int loc[2] = { ignicoes_passo, pegando_fogo };
            int glob[2];
            MPI_Allreduce(loc, glob, 2, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

            // Registra as novas ignições e o passo com o maior número delas
            s->ignicoes += glob[0];
            if (glob[0] > s->max_ignicoes) {
                s->max_ignicoes = glob[0];
                s->passo_mais_ignicoes = passo;
            }

            // Troca os vetores atuais pelos calculados neste passo
            unsigned char *te = t->estado;      t->estado      = t->proximo_estado; t->proximo_estado = te;
            unsigned char *tt = t->tempo_atual; t->tempo_atual = t->proximo_tempo;  t->proximo_tempo  = tt;

            passo++;
            continuar = (glob[1] > 0);
            ignicoes_passo = 0;
            pegando_fogo = 0;

            trocar_halo(t->estado, C, L, viz_cima, viz_baixo);
        }
        // Aguarda a troca dos vetores e a atualização da condição de parada
        #pragma omp barrier
    }

    s->passos = passo;
    s->tempo_exec = omp_get_wtime() - inicio;
}

// Conta os estados finais e mostra os dados da simulação
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

    // Cria um valor para conferir se os resultados das versões coincidem
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
