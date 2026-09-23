/* cliente.h - o que os tres arquivos do cliente expoem uns aos outros.
 *   comunicacao.cpp  entra na memoria compartilhada, envia e espera
 *   requisicao.cpp   monta requisicoes a partir do texto digitado
 *   terminal.cpp     main e tudo que aparece na tela */

#ifndef CLIENTE_H
#define CLIENTE_H

#include "protocolo.h"

/* comunicacao.cpp */
bool     ipc_abrir(void);
void     ipc_fechar(void);
Resposta ipc_pedir(Requisicao r);

/* requisicao.cpp */
Operacao operacao_de(const char* texto);
bool     montar_requisicao(Requisicao* r, int n, char** campo);

#endif
