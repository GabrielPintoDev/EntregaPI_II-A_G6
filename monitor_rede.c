/*
 * Monitor de Rede Simples - Projeto Integrador II-A
 * Disciplinas: Pensamento Computacional + Fundamentos de Redes de Computadores
 *
 * Funcionalidades:
 *  1. Listar endereços IP das interfaces locais e identificar o tipo (IPv4/IPv6)
 *  2. Medir latência e taxa de pacotes perdidos até um host remoto (usa 'ping' do SO)
 *
 * Compilar (Linux):   gcc monitor_rede.c -o monitor_rede
 * Executar:            ./monitor_rede
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ==================================================================
 * BLOCO DEPENDENTE DE SISTEMA OPERACIONAL
 * No Windows usamos a API do Windows (iphlpapi). No Linux/Mac usamos
 * a API POSIX (getifaddrs). O restante do programa é igual nos dois.
 * ================================================================== */
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
    #pragma comment(lib, "iphlpapi.lib")
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <ifaddrs.h>
    #include <netdb.h>
    #include <sys/socket.h>
    #include <arpa/inet.h>
#endif

/* ---------- Estruturas de Controle: função que classifica o tipo de IP ---------- */
void identificar_tipo_ip(int familia) {
    if (familia == AF_INET) {
        printf("   Tipo: IPv4\n");
    } else if (familia == AF_INET6) {
        printf("   Tipo: IPv6\n");
    } else {
        printf("   Tipo: Desconhecido\n");
    }
}

/* ---------- Comando 1: Coleta de informações de endereços IP ---------- */
#ifdef _WIN32
/* Versão Windows: usa GetAdaptersAddresses da biblioteca iphlpapi */
void listar_ips_locais(void) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    ULONG tamanho_buffer = 15000;
    PIP_ADAPTER_ADDRESSES adaptadores = (IP_ADAPTER_ADDRESSES *)malloc(tamanho_buffer);
    int contador = 0;

    if (GetAdaptersAddresses(AF_UNSPEC, 0, NULL, adaptadores, &tamanho_buffer) != NO_ERROR) {
        printf("Erro ao obter interfaces de rede.\n");
        free(adaptadores);
        WSACleanup();
        return;
    }

    printf("\n===== Endereços IP das Interfaces Locais =====\n");

    /* Estrutura de repetição 'while' percorrendo a lista encadeada de adaptadores */
    PIP_ADAPTER_ADDRESSES adaptador_atual = adaptadores;
    while (adaptador_atual != NULL) {
        PIP_ADAPTER_UNICAST_ADDRESS endereco_unicast = adaptador_atual->FirstUnicastAddress;

        /* Estrutura de repetição 'while' percorrendo os enderecos desse adaptador */
        while (endereco_unicast != NULL) {
            char buffer_ip[INET6_ADDRSTRLEN];
            struct sockaddr *sa = endereco_unicast->Address.lpSockaddr;
            int familia = sa->sa_family;

            /* Estrutura de decisão 'if' para filtrar apenas IPv4 e IPv6 */
            if (familia == AF_INET) {
                struct sockaddr_in *sa4 = (struct sockaddr_in *)sa;
                inet_ntop(AF_INET, &(sa4->sin_addr), buffer_ip, sizeof(buffer_ip));
            } else if (familia == AF_INET6) {
                struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)sa;
                inet_ntop(AF_INET6, &(sa6->sin6_addr), buffer_ip, sizeof(buffer_ip));
            } else {
                endereco_unicast = endereco_unicast->Next;
                continue;
            }

            contador++;
            printf("\n%d) Interface: %ws\n", contador, adaptador_atual->FriendlyName);
            printf("   Endereco: %s\n", buffer_ip);
            identificar_tipo_ip(familia);

            endereco_unicast = endereco_unicast->Next;
        }
        adaptador_atual = adaptador_atual->Next;
    }

    if (contador == 0) {
        printf("Nenhum endereco IP encontrado.\n");
    }

    free(adaptadores);
    WSACleanup();
}

#else
/* Versão Linux/Mac: usa getifaddrs (POSIX) */
void listar_ips_locais(void) {
    struct ifaddrs *lista_interfaces, *iface;
    char endereco[INET6_ADDRSTRLEN];
    int contador = 0;

    if (getifaddrs(&lista_interfaces) == -1) {
        perror("Erro ao obter interfaces de rede");
        return;
    }

    printf("\n===== Endereços IP das Interfaces Locais =====\n");

    /* Estrutura de repetição 'for' percorrendo a lista encadeada de interfaces */
    for (iface = lista_interfaces; iface != NULL; iface = iface->ifa_next) {
        if (iface->ifa_addr == NULL)
            continue;

        int familia = iface->ifa_addr->sa_family;

        /* Estrutura de decisão 'if' para filtrar apenas IPv4 e IPv6 */
        if (familia == AF_INET || familia == AF_INET6) {
            void *endereco_ptr;

            if (familia == AF_INET) {
                struct sockaddr_in *sa = (struct sockaddr_in *)iface->ifa_addr;
                endereco_ptr = &(sa->sin_addr);
            } else {
                struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)iface->ifa_addr;
                endereco_ptr = &(sa6->sin6_addr);
            }

            inet_ntop(familia, endereco_ptr, endereco, sizeof(endereco));

            contador++;
            printf("\n%d) Interface: %s\n", contador, iface->ifa_name);
            printf("   Endereco: %s\n", endereco);
            identificar_tipo_ip(familia);
        }
    }

    if (contador == 0) {
        printf("Nenhum endereco IP encontrado.\n");
    }

    freeifaddrs(lista_interfaces);
}
#endif

/* ---------- Comando 2: Medição de latência e cálculo de pacotes perdidos ---------- */
void medir_latencia_e_perda(const char *host, int quantidade_pacotes) {
    char comando[256];
    char linha[512];
    FILE *saida_ping;

    float tempos[64];       /* guarda o tempo de cada resposta recebida */
    int respostas_ok = 0;   /* quantas respostas realmente chegaram */
    int linhas_lidas = 0;

    /* Monta o comando de ping de acordo com o sistema operacional */
#ifdef _WIN32
    snprintf(comando, sizeof(comando), "ping -n %d %s", quantidade_pacotes, host);
#else
    snprintf(comando, sizeof(comando), "ping -c %d -W 2 %s 2>/dev/null", quantidade_pacotes, host);
#endif

    saida_ping = popen(comando, "r");
    if (saida_ping == NULL) {
        printf("Erro ao executar o ping. Verifique se o comando 'ping' esta disponivel.\n");
        return;
    }

    printf("\n===== Testando conectividade com %s =====\n", host);
    printf("----- Saida bruta do ping -----\n");

    /* Estrutura de repetição 'while' lendo linha por linha a saida do ping */
    while (fgets(linha, sizeof(linha), saida_ping) != NULL) {
        linhas_lidas++;
        printf("%s", linha); /* mostra a saida original, util pra conferencia/relatorio */

        /* Uma resposta valida sempre tem "tempo=Xms" (Windows-PT) ou "time=Xms" (Windows-EN/Linux).
         * Nao exigimos TTL= porque respostas de ping IPv6 no Windows nao mostram TTL. */
        char *pos_tempo = strstr(linha, "tempo=");
        if (pos_tempo == NULL) pos_tempo = strstr(linha, "time=");

        if (pos_tempo != NULL && respostas_ok < 64) {
            char *igual = strchr(pos_tempo, '=');
            float valor = -1;
            if (igual != NULL && sscanf(igual + 1, "%f", &valor) == 1) {
                tempos[respostas_ok] = valor;
                respostas_ok++;
            }
        }
    }

    pclose(saida_ping);

    if (linhas_lidas == 0) {
        printf("\nAtencao: o comando 'ping' nao retornou nenhuma linha.\n");
        printf("Isso costuma acontecer em compiladores/ambientes online que bloqueiam acesso a rede.\n");
        printf("Rode este programa localmente (seu PC) para o teste de latencia funcionar.\n");
        return;
    }

    /* ---- Calculo manual de estatisticas a partir das respostas coletadas ---- */
    float perda_percentual = ((float)(quantidade_pacotes - respostas_ok) / quantidade_pacotes) * 100.0f;

    printf("\n----- Resultado -----\n");
    printf("Pacotes enviados : %d\n", quantidade_pacotes);
    printf("Respostas obtidas: %d\n", respostas_ok);
    printf("Taxa de pacotes perdidos: %.1f%%\n", perda_percentual);

    if (respostas_ok > 0) {
        float soma = 0, minimo = tempos[0], maximo = tempos[0];

        /* Estrutura de repetição 'for' calculando min/max/media */
        for (int i = 0; i < respostas_ok; i++) {
            soma += tempos[i];
            if (tempos[i] < minimo) minimo = tempos[i];
            if (tempos[i] > maximo) maximo = tempos[i];
        }

        printf("Latencia minima : %.2f ms\n", minimo);
        printf("Latencia media  : %.2f ms\n", soma / respostas_ok);
        printf("Latencia maxima : %.2f ms\n", maximo);
    } else {
        printf("Nenhuma resposta recebida - nao foi possivel medir latencia.\n");
        printf("Possiveis causas: host inacessivel, firewall bloqueando ICMP, ou sem conexao com a internet.\n");
    }
}

/* ---------- Programa Principal ---------- */
int main(void) {
    int opcao;
    char host[128];
    int continuar = 1;

    printf("############################################\n");
    printf("#        MONITOR DE REDE SIMPLES (C)      #\n");
    printf("############################################\n");
    printf("Estas sao as informacoes do monitor de rede do grupo G6:\n");

    /* Estrutura de repetição 'while' para o menu principal */
    while (continuar) {
        printf("\n===== MENU =====\n");
        printf("1. Listar enderecos IP locais (IPv4/IPv6)\n");
        printf("2. Medir latencia e taxa de pacotes perdidos\n");
        printf("3. Sair\n");
        printf("Escolha uma opcao: ");

        if (scanf("%d", &opcao) != 1) {
            printf("Entrada invalida.\n");
            /* limpa o buffer de entrada */
            while (getchar() != '\n');
            continue;
        }

        /* Estrutura de decisão 'if/else if' para tratar a opcao escolhida */
        if (opcao == 1) {
            listar_ips_locais();
        } else if (opcao == 2) {
            printf("Digite o endereco/host a testar (ex: 8.8.8.8 ou google.com): ");
            scanf("%127s", host);
            medir_latencia_e_perda(host, 4);
        } else if (opcao == 3) {
            printf("Encerrando o monitor de rede...\n");
            continuar = 0;
        } else {
            printf("Opcao invalida. Tente novamente.\n");
        }
    }

    return 0;
}
