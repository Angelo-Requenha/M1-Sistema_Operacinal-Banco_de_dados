/* servidor.h - o que os tres arquivos do servidor expoem uns aos outros.
 *   comunicacao.cpp  memoria compartilhada e pool de threads
 *   banco.cpp        tabela, mutex e log
 *   terminal.cpp     main e tudo que aparece na tela */

#ifndef SERVIDOR_H
#define SERVIDOR_H

#include "protocolo.h"

/* comunicacao.cpp */
bool ipc_criar(int num_threads);
void ipc_rodar_pool(void);
void ipc_pedir_parada(void);
void ipc_destruir(void);

/* banco.cpp - tabela */
void     banco_definir_custo(int ms);
void     banco_carregar(void);
Resposta banco_processar(const Requisicao& req, int id_thread);
int      banco_total(void);
long     banco_contagem(Operacao op);

/* banco.cpp - log (arquivo) */
void log_abrir(void);
void log_fechar(void);
void log_escrever(const char* fmt, ...);

#endif
