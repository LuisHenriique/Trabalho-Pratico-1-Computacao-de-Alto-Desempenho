## Metodologia PCAM Aplicada ao Simulador de Incêndio Florestal

O projeto da solução híbrida **MPI + OpenMP** seguiu as quatro etapas da metodologia **PCAM** (*Partitioning, Communication, Agglomeration, Mapping*), visando maximizar a localidade espacial dos dados, minimizar a latência de rede entre nós e equilibrar a carga computacional entre núcleos.

---

### 1. Particionamento (*Partitioning*)
O problema foi particionado sob a abordagem de **decomposição de domínio** (*domain decomposition*). A unidade atômica de computação é cada célula $(l, c)$ da grade bidimensional $L \times C$, cujo próximo estado depende apenas do seu estado atual, de propriedades locais estáticas (`cobertura`, `umidade`, `ativacao`) e dos estados de seus 8 vizinhos na **vizinhança de Moore** (raio 1), modulados pelo vetor global de vento.

* **Particionamento de Dados e Tarefas:** Como os vetores de `cobertura`, `umidade` e `ativacao` são imutáveis após a inicialização (com exceção da consulta de ativação no passo $p$), o foco do particionamento recai sobre as matrizes dinâmicas de estado (`estado`, `proximo_estado`) e tempo de queima (`tempo_atual`, `proximo_tempo`).
* **Granularidade Fina Inicial:** Conceitualmente, cada célula define uma tarefa independente dentro de um mesmo passo temporal $p$, garantindo ausência de dependências de escrita (*write-after-write*) graças à técnica de *double buffering* (`estado` $\rightarrow$ `proximo_estado`).

---

### 2. Comunicação (*Communication*)
A análise das dependências de dados revelou dois padrões distintos de comunicação ao longo de cada passo temporal $p$:

1. **Comunicação Local (Ponto a Ponto — *Halo Exchange*):**
   Para atualizar uma célula $(l, c)$, é necessário ler os estados atuais das 8 células adjacentes. Quando o domínio é dividido entre processos distintos, células situadas nas fronteiras de uma partição necessitam de dados residentes nos processos vizinhos. Isso caracteriza uma comunicação local, restrita apenas às linhas de borda adjacentes.
2. **Comunicação Global (Coletiva):**
   * **Distribuição e Coleta Inicial/Final:** O processo mestre (`rank 0`) realiza a leitura do arquivo de entrada e a geração pseudoaleatória determinística (`rand_r`), difundindo os parâmetros escalares via `MPI_Bcast` e distribuindo as fatias da matriz via `MPI_Scatterv`. Ao término da simulação, os estados finais são reunidos via `MPI_Gatherv` para o cálculo do *checksum* sequencial.
   * **Sincronização e Redução por Passo:** Ao final de cada passo $p$, todos os processos precisam somar as novas ignições locais (`ignicoes_passo`) e verificar se ainda há alguma célula em chamas (`pegando_fogo > 0`) em qualquer região da floresta para decidir a condição de parada. Isso é resolvido com uma única operação coletiva `MPI_Allreduce`.

---

### 3. Aglomeração (*Agglomeration*)
Manter cada célula como uma tarefa MPI isolada geraria um *overhead* proibitivo de latência de rede e memória. A aglomeração foi realizada em dois níveis hierárquicos:

* **Nível Inter-nó (Aglomeração em Blocos Contíguos de Linhas para MPI):**
  As $L$ linhas da matriz são aglomeradas em $P_{\text{MPI}}$ fatias horizontais contíguas na memória (padrão *row-major* do C). Cada processo recebe $\lfloor L / P_{\text{MPI}} \rfloor$ linhas, distribuindo o resto $L \bmod P_{\text{MPI}}$ aos primeiros processos (uma linha extra por processo até zerar o resto).
  * *Vantagem:* Reduz a comunicação ponto a ponto a apenas **duas linhas inteiras** por processo a cada passo (*ghost rows* superior e inferior), enviadas em mensagens contíguas únicas (`MPI_Isend`/`MPI_Irecv`), eliminando empacotamento manual e maximizando o aproveitamento de cache.
* **Nível Intra-nó (Aglomeração para OpenMP e Vetorização):**
  Dentro de cada processo MPI, a fatia local (`local_L` linhas) é subdividida entre $T$ threads OpenMP em uma **região paralela persistente** (aberta uma única vez fora do laço temporal para evitar o custo de *fork/join* a cada passo). Além disso, as linhas internas da fatia local são computadas enquanto a comunicação não bloqueante (*halo exchange*) ocorre em segundo plano, sobrepondo computação e comunicação.

---

### 4. Mapeamento (*Mapping*)
O mapeamento associa as tarefas aglomeradas aos recursos físicos do *cluster*:

* **Processos MPI $\rightarrow$ Nós Computacionais / Sockets:** Cada processo MPI é mapeado aos nós disponíveis via `--hostfile` (`mpirun -np NPrc`), mantendo cada fatia do terreno inteiramente na memória RAM local do respectivo nó.
* **Threads OpenMP $\rightarrow$ Núcleos Físicos (*Cores*):** Cada processo MPI instancia $T$ threads (`OMP_NUM_THREADS=T` / `num_threads(T)`) utilizando o nível de suporte `MPI_THREAD_FUNNELED`. Dessa forma, todas as $T$ threads executam processamento paralelo de memória compartilhada nos núcleos locais, enquanto apenas a thread principal (`#pragma omp master`) realiza chamadas MPI, evitando contenção na biblioteca de comunicação e maximizando a afinidade de memória NUMA.
* **Balanceamento de Carga:** Como o custo computacional por célula é relativamente uniforme (e o avanço da frente de fogo varia dinamicamente ao longo do tempo), a divisão equilibrada de linhas (`local_L` variando no máximo em 1 linha entre processos) combinada ao escalonamento OpenMP mantém a carga bem distribuída sem necessidade de migração dinâmica de dados entre nós.
