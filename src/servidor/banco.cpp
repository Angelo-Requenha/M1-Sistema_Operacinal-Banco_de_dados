/* servidor/banco.cpp - a tabela compartilhada pelas threads do pool.
 * Contem a regra de negocio (INSERT/SELECT/UPDATE/DELETE/LISTAR), a
 * exclusao mutua sobre a tabela e o log em arquivo. Nao imprime nada
 * na tela: quem imprime e terminal.cpp. */

#include "servidor.h"

#include <pthread.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <ctime>

static Registro tabela[MAX_REGISTROS];
static int      total_registros = 0;

/* Exclusao mutua simples: so uma thread por vez mexe na tabela, seja
 * leitura ou escrita. Mais simples que leitores/escritores e ainda
 * garante integridade dos dados. */
static pthread_mutex_t mtx_tabela = PTHREAD_MUTEX_INITIALIZER;

static long stat_op[OP_ENCERRAR + 1] = {0};
static pthread_mutex_t mtx_stat = PTHREAD_MUTEX_INITIALIZER;

static int custo_ms = 0;

static FILE* arq_log = nullptr;
static pthread_mutex_t mtx_log = PTHREAD_MUTEX_INITIALIZER;

/* ---------------------------------------------------------------- log ---*/

void log_abrir(void)
{
    arq_log = fopen(ARQUIVO_LOG, "a");
    if (!arq_log) perror("aviso: nao abri o arquivo de log");
}

void log_fechar(void)
{
    if (arq_log) fclose(arq_log);
    arq_log = nullptr;
}

void log_escrever(const char* fmt, ...)
{
    char corpo[TAM_LINHA_LOG];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(corpo, sizeof corpo, fmt, ap);
    va_end(ap);

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tmv;
    localtime_s(&tmv, &ts.tv_sec);

    char hora[16];
    strftime(hora, sizeof hora, "%H:%M:%S", &tmv);

    pthread_mutex_lock(&mtx_log);
    if (arq_log) {
        fprintf(arq_log, "[%s.%03ld] %s\n", hora, ts.tv_nsec / 1000000L, corpo);
        fflush(arq_log);
    }
    pthread_mutex_unlock(&mtx_log);
}

/* -------------------------------------------------------- persistencia --*/

void banco_carregar(void)
{
    FILE* f = fopen(ARQUIVO_BANCO, "r");
    if (!f) {
        log_escrever("BOOT  %s inexistente, tabela vazia", ARQUIVO_BANCO);
        return;
    }

    char linha[TAM_NOME + 32];
    while (fgets(linha, sizeof linha, f) && total_registros < MAX_REGISTROS) {
        linha[strcspn(linha, "\r\n")] = '\0';
        if (linha[0] == '\0' || linha[0] == '#') continue;

        char* sep = strchr(linha, ';');
        if (!sep) continue;
        *sep = '\0';

        tabela[total_registros].id = atoi(linha);
        snprintf(tabela[total_registros].nome, TAM_NOME, "%s", sep + 1);
        total_registros++;
    }
    fclose(f);

    log_escrever("BOOT  %d registro(s) carregado(s) de %s",
                 total_registros, ARQUIVO_BANCO);
}

/* So e chamada com o mutex ja travado */
static void salvar_banco(void)
{
    FILE* f = fopen(ARQUIVO_BANCO, "w");
    if (!f) {
        log_escrever("ERRO  nao consegui gravar %s", ARQUIVO_BANCO);
        return;
    }
    fprintf(f, "# id;nome - gerado pelo servidor\n");
    for (int i = 0; i < total_registros; i++)
        fprintf(f, "%d;%s\n", tabela[i].id, tabela[i].nome);
    fclose(f);
}

static int indice_de(int id)
{
    for (int i = 0; i < total_registros; i++)
        if (tabela[i].id == id) return i;
    return -1;
}

/* ---------------------------------------------------------- operacoes ---*/

void banco_definir_custo(int ms) { custo_ms = ms; }

/* Atraso artificial simulando I/O do banco. Com varias threads e um
 * custo perceptivel (ex.: 500ms), da pra ver no log duas requisicoes
 * de clientes diferentes esperando uma a outra, provando a exclusao
 * mutua sobre a tabela. */
static void gastar_tempo(void)
{
    if (custo_ms <= 0) return;
    struct timespec t = { custo_ms / 1000, (custo_ms % 1000) * 1000000L };
    nanosleep(&t, nullptr);
}

static void fazer_select(const Requisicao& req, Resposta& res)
{
    int i = indice_de(req.id);
    if (i < 0) {
        snprintf(res.texto, TAM_TEXTO, "SELECT id=%d -> nao encontrado", req.id);
        return;
    }
    res.ok = 1;
    snprintf(res.nome,  TAM_NOME,  "%s", tabela[i].nome);
    snprintf(res.texto, TAM_TEXTO, "SELECT id=%d -> nome=%s",
             req.id, tabela[i].nome);
}

static void fazer_insert(const Requisicao& req, Resposta& res)
{
    if (indice_de(req.id) >= 0) {
        snprintf(res.texto, TAM_TEXTO, "INSERT id=%d -> id ja existe", req.id);
        return;
    }
    if (total_registros >= MAX_REGISTROS) {
        snprintf(res.texto, TAM_TEXTO, "INSERT id=%d -> tabela cheia", req.id);
        return;
    }

    tabela[total_registros].id = req.id;
    snprintf(tabela[total_registros].nome, TAM_NOME, "%s", req.nome);
    total_registros++;
    salvar_banco();

    res.ok = 1;
    snprintf(res.nome,  TAM_NOME,  "%s", req.nome);
    snprintf(res.texto, TAM_TEXTO, "INSERT id=%d nome=%s -> ok",
             req.id, req.nome);
}

static void fazer_update(const Requisicao& req, Resposta& res)
{
    int i = indice_de(req.id);
    if (i < 0) {
        snprintf(res.texto, TAM_TEXTO, "UPDATE id=%d -> nao encontrado", req.id);
        return;
    }

    snprintf(tabela[i].nome, TAM_NOME, "%s", req.nome);
    salvar_banco();

    res.ok = 1;
    snprintf(res.nome,  TAM_NOME,  "%s", req.nome);
    snprintf(res.texto, TAM_TEXTO, "UPDATE id=%d -> nome=%s",
             req.id, req.nome);
}

static void fazer_delete(const Requisicao& req, Resposta& res)
{
    int i = indice_de(req.id);
    if (i < 0) {
        snprintf(res.texto, TAM_TEXTO, "DELETE id=%d -> nao encontrado", req.id);
        return;
    }

    for (int k = i; k < total_registros - 1; k++)
        tabela[k] = tabela[k + 1];
    total_registros--;
    salvar_banco();

    res.ok = 1;
    snprintf(res.texto, TAM_TEXTO, "DELETE id=%d -> removido", req.id);
}

/* Toda operacao sobre a tabela (leitura ou escrita) entra e sai do mutex
 * da mesma forma: so uma thread mexe na tabela por vez. */
Resposta banco_processar(const Requisicao& req, int id_thread)
{
    Resposta res;
    memset(&res, 0, sizeof res);
    res.id             = req.id;
    res.thread_atendeu = id_thread;
    res.ok             = 0;

    switch (req.op) {

    case OP_SELECT:
        pthread_mutex_lock(&mtx_tabela);
        gastar_tempo();
        fazer_select(req, res);
        pthread_mutex_unlock(&mtx_tabela);
        break;

    case OP_LISTAR:
        pthread_mutex_lock(&mtx_tabela);
        gastar_tempo();
        res.ok = 1;
        snprintf(res.texto, TAM_TEXTO, "LISTAR -> %d registro(s) na tabela",
                 total_registros);
        pthread_mutex_unlock(&mtx_tabela);
        break;

    case OP_INSERT:
        pthread_mutex_lock(&mtx_tabela);
        gastar_tempo();
        fazer_insert(req, res);
        pthread_mutex_unlock(&mtx_tabela);
        break;

    case OP_UPDATE:
        pthread_mutex_lock(&mtx_tabela);
        gastar_tempo();
        fazer_update(req, res);
        pthread_mutex_unlock(&mtx_tabela);
        break;

    case OP_DELETE:
        pthread_mutex_lock(&mtx_tabela);
        gastar_tempo();
        fazer_delete(req, res);
        pthread_mutex_unlock(&mtx_tabela);
        break;

    case OP_ENCERRAR:
        res.ok = 1;
        snprintf(res.texto, TAM_TEXTO, "ENCERRAR -> servidor desligando");
        break;

    default:
        snprintf(res.texto, TAM_TEXTO, "operacao desconhecida (%d)",
                 (int)req.op);
        break;
    }

    pthread_mutex_lock(&mtx_stat);
    if (req.op >= 0 && req.op <= OP_ENCERRAR) stat_op[req.op]++;
    pthread_mutex_unlock(&mtx_stat);

    return res;
}

int  banco_total(void)           { return total_registros; }
long banco_contagem(Operacao op) { return stat_op[op]; }
