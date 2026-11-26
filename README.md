# Trabalho Prático 1 - Computação de Alto Desempenho

# Guia Rápido da Estrutura (`main`) e Experimentos — Simulador Híbrido (MPI + OpenMP)

## 1. Como a `main` está funcionando

A função `main` segue o modelo mestre-trabalhador apenas para I/O e inicialização, passando para SPMD (todos executam o mesmo código em fatias diferentes) durante a simulação:

1. **Inicialização Híbrida (`MPI_Init_thread`):**
   - Iniciamos o MPI com suporte **`MPI_THREAD_FUNNELED`**, garantindo que múltiplas threads OpenMP rodem dentro de cada processo, mas **apenas a thread principal (`master`)** faça chamadas MPI.
   - Recuperamos o `rank` (ID do processo) e o `size` (total de processos passados no `-np`).

2. **Leitura e Geração Determinística apenas no `rank 0`:**
   - Somente o `rank 0` lê o arquivo de entrada, valida os parâmetros e gera o mapa inicial (`preparar_mundo`), preservando o determinismo do `rand_r(&seed)`.
   - Os 8 parâmetros globais (`L`, `C`, `max_passos`, `threads`, `limiar`, `intensidade`, `dir_linha`, `dir_coluna`) são empacotados em `params[8]` e enviados para todos os processos via **`MPI_Bcast`**.

3. **Decomposição por Linhas (`sendcounts` e `displs`):**
   - Cada processo calcula quantas linhas vai receber (`n_linhas_p = L / size + (rank < L % size ? 1 : 0)`). O resto da divisão inteira é distribuído (1 linha extra por processo) para os primeiros ranks.
   - O `rank 0` monta os vetores `sendcounts` e `displs` (multiplicando as linhas por `C`, já que o MPI conta elementos/células).

4. **Alocação Local e *Ghost Rows* (`aloca_arrays_locais` + `MPI_Scatterv`):**
   - Cada processo aloca sua fatia local (`mapa_local`).
   - Os vetores estáticos/individuais (`cobertura`, `umidade`, `ativacao`, `tempo_atual`, `proximo_tempo`) têm tamanho `n_linhas_p * C`.
   - Os vetores de estado (`estado` e `proximo_estado`) têm tamanho **`(n_linhas_p + 2) * C`**, reservando a **linha `0`** (topo) e a **linha `n_linhas_p + 1`** (base) como *ghost rows* para troca de fronteira entre processos vizinhos.
   - Na hora do `MPI_Scatterv`, o `estado` local é preenchido a partir de `&mapa_local.estado[C]` (linha 1) para não sobrescrever a *ghost row* superior.

5. **Coleta Final e Verificação (`MPI_Gatherv`):**
   - Após o retorno da função `simular`, todos os processos enviam suas linhas reais (`&mapa_local.estado[C]` e `mapa_local.tempo_atual`) de volta para o `rank 0` via **`MPI_Gatherv`**.
   - Somente o `rank 0` imprime as estatísticas finais e calcula o `checksum` sequencial para validação.

---

## 2. O que vocês podem mudar para testar opções e otimizações

Para os experimentos do relatório e ajustes de desempenho no cluster, os principais pontos de variação são:

### A. Combinações de Processos MPI (`-np`) vs. Threads OpenMP (`T`)
Mantendo o número total de núcleos físicos fixo (ex: 16 ou 32 cores no cluster), testem diferentes proporções mantendo o arquivo de entrada ou alterando o `T` (e `-np` na linha de comando):
- **Puro OpenMP (baseline paralelo):** `-np 1` com `T = max_cores`
- **Híbrido Balanceado:** ex. `-np 2` com `T = 8`, `-np 4` com `T = 4` (geralmente 1 processo MPI por *socket* NUMA dá ótimos resultados)
- **Puro MPI:** `-np max_cores` com `T = 1`

### B. Políticas de Escalonamento do OpenMP (`schedule`)
Nos laços `#pragma omp for` dentro da simulação, vale testar e comparar os tempos de execução com:
- `schedule(static)` — menor *overhead*, ótimo quando a carga é uniforme.
- `schedule(dynamic, chunk)` ou `schedule(guided)` — pode ajudar se o fogo ficar muito concentrado em uma região da matriz (desbalanceamento de carga entre threads do mesmo processo), testando tamanhos de `chunk` como `1`, `16` ou `64`.
- Dica: usar `schedule(runtime)` no código permite variar isso direto no terminal exportando `OMP_SCHEDULE="dynamic,16"` sem precisar recompilar!

### C. Flags de Compilação e Vetorização (`Makefile`)
Testem o impacto das otimizações do compilador na vetorização (`#pragma omp simd`) e no tempo total:
- Comparar `-O2` vs. `-O3`
- Testar `-march=native` (habilita instruções AVX2/AVX-512 específicas dos nós do cluster)
- Verificar relatório de vetorização com `-fopt-info-vec-optimized` (no GCC) para confirmar se os laços internos estão realmente vetorizando.

### D. Tamanho do Problema (Escalabilidade Forte e Fraca)
- **Escalabilidade Forte (*Strong Scaling*):** Fixar uma grade grande (ex: `4000 x 4000` ou `8000 x 8000`) e aumentar o total de recursos (`1, 2, 4, 8, 16, 32` cores) medindo *Speedup* e Eficiência.
- **Escalabilidade Fraca (*Weak Scaling*):** Aumentar o número de linhas `L` proporcionalmente ao número de processos MPI (`-np`), mantendo `n_linhas_p` constante por nó para avaliar o custo da comunicação de borda e do `MPI_Allreduce`.

### E. Regra de Ouro nos Testes
> **Sempre validem o `checksum`!** Qualquer alteração de `schedule`, número de processos `-np` ou threads `T` **precisa** gerar exatamente o mesmo `checksum`, total de passos e contagem de células da versão sequencial para a mesma entrada.
