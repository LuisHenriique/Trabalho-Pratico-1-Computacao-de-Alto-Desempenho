## Metodologia PCAM Aplicada ao Simulador de Incêndio Florestal

O projeto da solução híbrida **MPI + OpenMP** seguiu as quatro etapas da metodologia **PCAM** (*Partitioning, Communication, Agglomeration, Mapping*), visando maximizar a localidade espacial dos dados, minimizar a latência de rede entre nós e equilibrar a carga computacional entre núcleos.

### Breve descrição do problema

Diferentes células de uma área florestal precisam ser analisadas;
Calcular a propagação do fogo com base nos vizinhos, no vento e nas características de cada célula;
Determinar se cada célula permanece intacta, entra em chamas, continua queimando, fica queimada ou se torna contenção;
Repetir a análise até o fogo acabar ou atingir o limite de passos;
Calcular e imprimir as estatísticas e o checksum da configuração final.

---

### 1. Particionamento (*Partitioning*)

O particionamento é **por dados**. A floresta é representada por uma matriz de $L \times C$, para cada uma das células da matriz, há uma tarefa de atualização  $(l, c)$ em cada passo $p$.

Há uma única tarefa responsável pela **leitura e preparação** do terreno. Após a leitura, ela fornece para as outras tarefas: configuração inicial das células, focos de incêndio e programação das contenções às tarefas de atualização.

As tarefas de **atualização das células** podem ser executadas em paralelo em um mesmo passo. Cada uma determina o próximo estado e o próximo tempo de queima de sua célula, considerando suas características, a contenção programada e, quando necessário, os estados dos oito vizinhos e o vento. Graças à técnica de *double buffering* (`estado` $\rightarrow$ `proximo_estado`), ada célula define uma tarefa independente dentro de um mesmo passo temporal $p$, garantindo ausência de dependências de escrita (*write-after-write*).

Após as atualizações, há uma tarefa de **consolidação das informações do passo**, que depende dos resultados das tarefas por célula. Ela reúne as novas ignições e a informação de existência de fogo, atualiza as estatísticas de ignição e determina a continuidade da simulação. Assim, **as células de um mesmo passo podem ser atualizadas em paralelo, mas os passos possuem dependência entre si**.

Há uma tarefa de **finalização e saída**, que depende da conclusão da simulação. Ela utiliza a configuração final e as estatísticas acumuladas para calcular as contagens finais, os percentuais e o *checksum* e imprimir os resultados.

---


### 2. Comunicação (*Communication*)

O fluxo de comunicações segue o grafo representado pela *Figura 1*. Começando pela tarefa de **leitura e preparação** (`rank 0`), fornece os parâmetros gerais da simulação e os dados iniciais das células às tarefas de atualização via `MPI_Bcast` e dsictribui a matriz usando `MPI_Scatterv`.

Durante a simulação, a análise das dependências de dados revelou dois padrões distintos de comunicação ao longo de cada passo temporal $p$. 

* **Comunicação Local (*Halo Exchange*):** A atualização de uma célula depende dos estados atuais de seus 8 vizinhos. Quando regiões são divididos em processos diferentes, os estados das células vizinhas precisam ser enviados antes de sua utilização. Na divisão por faixas de linhas usada na solução, essa troca envolve as linhas de fronteira entre faixas adjacentes.

* **Comunicação Global:** Para garantir a simulação correta, ao final de cada passo $p$ ocorre **uma sincronização**, garantindo a completude do passo antes da troca dos estados atuais pelos próximos e os estados. Nesse momento ocorre a verificar de células em chamas (`pegando_fogo > 0`) e a atualização de (`ignicoes_passo`). O `MPI_Allreduce` foi usado para tratar isso.
 
Ao encerrar a simulação, os estados e tempos de queima finais são enviados à tarefa de **finalização e saída** via `MPI_Gatherv`. A organização original das células é preservada para que o *checksum* seja calculado sequencialmente.

---

### 3. Aglomeração (*Agglomeration*)

Manter cada célula como uma tarefa MPI isolada geraria uma *overhead* de comunicação e gerenciamento. Para aumentar a granularidade, as tarefas são agrupadas em dois níveis: **entre processos MPI** e **entre threads OpenMP de um mesmo processo**.

Sobre a **aglomeração em blocos contíguos de linhas para MPI:** as $L$ linhas da matriz são agrupadas em $P_{\text{MPI}}$ faixas horizontais contíguas na memória, sendo $P_{\text{MPI}}$ o número de processos iniciado pelo `mpirun`. Cada processo recebe $\lfloor L / P_{\text{MPI}} \rfloor$ linhas, e as linhas restantes são distribuídas, uma por processo, aos primeiros ranks. A *vantagem* é que agrupar células vizinhas no mesmo processo mantém suas consultas na memória local e limita a comunicação com outras faixas às linhas de fronteira superior e inferior. Os estados dessas linhas são trocados com `MPI_Sendrecv` e armazenados nas linhas extras locais (*ghost rows*).

Já a **aglomeração dentro do processo para OpenMP e vetorização**, ocorre dentro de cada processo MPI, as tarefas das células da faixa local são divididas em grupos entre $T$ threads OpenMP, com escalonamento estático. A equipe é mantida em uma **região paralela persistente**, aberta fora do laço de passos, para evitar o custo de criação e encerramento das threads a cada passo. O laço utiliza também `simd` para permitir a vetorização das atualizações.

As novas ignições e a presença de fogo são combinadas primeiro entre as threads de cada processo. Os dois resultados locais são agrupados em uma única redução MPI por passo, reduzindo a quantidade de operações coletivas. A thread principal realiza as chamadas MPI e a troca dos vetores, enquanto as demais aguardam essa etapa.

A leitura, a preparação da floresta e a saída são agrupadas no processo de `rank 0`, que também recebe uma faixa da matriz e participa da simulação.

---

### 4. Mapeamento (*Mapping*)
O mapeamento associa as tarefas aglomeradas aos recursos físicos do *cluster*:

* **Processos MPI $\rightarrow$ Nós Computacionais / Sockets:** Cada processo MPI é mapeado aos nós disponíveis via `--hostfile` (`mpirun -np NPrc`), mantendo cada fatia do terreno inteiramente na memória RAM local do respectivo nó.
* **Threads OpenMP $\rightarrow$ Núcleos Físicos (*Cores*):** Cada processo MPI instancia $T$ threads (`OMP_NUM_THREADS=T` / `num_threads(T)`) utilizando o nível de suporte `MPI_THREAD_FUNNELED`. Dessa forma, todas as $T$ threads executam processamento paralelo de memória compartilhada nos núcleos locais, enquanto apenas a thread principal (`#pragma omp master`) realiza chamadas MPI, evitando contenção na biblioteca de comunicação e maximizando a afinidade de memória NUMA.
* **Balanceamento de Carga:** Como o custo computacional por célula é relativamente uniforme (e o avanço da frente de fogo varia dinamicamente ao longo do tempo), a divisão equilibrada de linhas (`local_L` variando no máximo em 1 linha entre processos) combinada ao escalonamento OpenMP mantém a carga bem distribuída sem necessidade de migração dinâmica de dados entre nós.
