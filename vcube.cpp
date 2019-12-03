/*
=============================================================================
Codigo fonte: implementacao do broadcast confiavel hierarquico com V-Cube.
Codigo fonte: implementacao do Paxos
Autora: Acacia dos Campos da Terra
Orientador: Prof. Elias P. Duarte Jr.
=============================================================================
*/

/*--------bibliotecas--------*/
#include <stdio.h>
#include <stdlib.h>
#include "smpl.h"
#include <set>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <memory>

/*--------eventos--------*/
#define test 1
#define fault 2
#define repair 3
#define propose 4
#define etapa1 5
#define etapa2 6
#define etapa3 7
#define etapa4 8
#define receive_ack 9
#define broadcast_prep 10
#define new_msg 11
#define broadcast_acc 12
#define receive_ack_acc 13

/*--------definicoes usadas pela funcao cisj--------*/
#define POW_2(num) (1<<(num)) //equivale a 2 elevado a num (sendo num o tamanho dos clusters)
#define VALID_J(j, s) ((POW_2(s-1)) >= j) //verifica se j eh um indice valido para acessar no cluster

/*--------estrutura das mensagens(tree e ack)--------*/
typedef struct {
	char type; //2 tipos: tree e ack (T e A) + P (prepare request) e R (accept request)
	std:: string m; //mensagem m
	int idorigem; //id da origem
	int timestamp; //contador sequencial

	int numproposta; //numero de proposta paxos
	int valproposta; //valor associado ao numero de proposta aceito
}tmsg;
std:: vector<tmsg> mensagens;

//ultima mensagem utilizada pelo programa (para enviar/receber)
// auto ultima_msg = std::make_unique<tmsg>();
std:: vector <tmsg> ult_prepare;
std:: vector <tmsg> ult_accept;

/*--------estrutura da tripla armazenada pelo ack_set--------*/
struct tset{
	int idorigem, iddestino;
	tmsg m;

	bool operator<(const tset &outro) const{
		if(idorigem < outro.idorigem)
			return true;
		if(outro.idorigem < idorigem)
			return false;
		if(iddestino < outro.iddestino)
			return true;
		if(outro.iddestino < iddestino)
			return false;
		return m.m < outro.m.m;
	}
};

/*--------descritor do nodo--------*/
typedef struct{
	int id; //cada nodo tem um id
	int idr; //id real!!!
	int *state; //vetor state do nodo
	std:: vector<tmsg> last; //vetor com a ultima mensagem recebida de cada processo fonte
	std:: set<tset> ack_set; //set de tset
	std:: vector<int> papel; //cada processo pode assumir um ou mais papeis
    int reg; // se o 'registrador' for 0, significa que esse processo ainda nao decidiu
}tnodo;
std:: vector<tnodo> nodo;
//------------- paxos
std:: vector< std:: pair<int, int>> propostas;
// std:: vector<tnodo> nodo;
std:: vector<std:: pair<int, int>> maiornumerorecebido;
// blablabla
std:: vector< std:: vector <std:: pair<int, int>>> respostas;
std:: vector< std::vector <int>> quemrespondeu;
std:: vector<int> numeros;
std:: vector<int> passoupela3;
//contresp eh responsavel por armazenar quem respondeu ao prepare request
std:: vector<std:: vector<int>> contresp;
int enviados = 0;
int npropostaatual = 0;
//-------------- paxos

/*--------estrutura do vetor de testes--------*/
typedef struct{
	int *vector; //cada nodo retornara um vetor de inteiros
	int size; //tamanho do vetor
	int *etapa;//o s da funcao cis
} testsvector;
testsvector *tests;

/*--------estrutura usada pela funcao cisj--------*/
typedef struct node_set {
	int* nodes;
	ssize_t size;
	ssize_t offset;
} node_set;

int token, N, timestamp = 0, s, ultimo_nodo;
/*--------funcoes usadas pelo cisj--------*/
static node_set*
set_new(ssize_t size)
{
	node_set* newn;

	newn = (node_set*)malloc(sizeof(node_set));
	newn->nodes = (int*)malloc(sizeof(int)*size);
	newn->size = size;
	newn->offset = 0;
	return newn;
}

static void
set_insert(node_set* nodes, int node)
{
	if (nodes == NULL) return;
	nodes->nodes[nodes->offset++] = node;
}

static void
set_merge(node_set* dest, node_set* source)
{
	if (dest == NULL || source == NULL) return;
	memcpy(&(dest->nodes[dest->offset]), source->nodes, sizeof(int)*source->size);
	dest->offset += source->size;
}

static void
set_free(node_set* nodes)
{
	free(nodes->nodes);
	free(nodes);
}
/* node_set.h --| */

node_set*
cis(int i, int s)
{
	node_set* nodes, *other_nodes;
	int xorr = i ^ POW_2(s-1);
	int j;

	/* starting node list */
	nodes = set_new(POW_2(s-1));

	/* inserting the first value (i XOR 2^^(s-1)) */
	set_insert(nodes, xorr);

	/* recursion */
	for (j=1; j<=s-1; j++) {
		other_nodes = cis(xorr, j);
		set_merge(nodes, other_nodes);
		set_free(other_nodes);
	}
	return nodes;
}
/*--------fim das funcoes do cisj--------*/


/*--------funcao que encontra qual sera o nodo testador--------*/
//nesta funcao sao encontrados os testadores de cada nodo
//(primeiro nodo SEM-FALHA de cada cluster em cada etapa)
int findtester (int i, int s) {
	int aux = 0;
	node_set* cluster = cis(i, s);
	// printf("---------> i: %d s: %d cluster: %d\n", i, s, cluster->offset);
	for(int j = 0; j < cluster->size; j++){ //for para percorrer todo o cluster
		if(status(nodo[cluster->nodes[j]].id) == 0){//verifica se o nodo encontrado eh sem-falha
			//ira retornar o primeiro do cluster que esta SEM-FALHA
			aux = cluster->nodes[j];
			free(cluster); //libera a memoria
			return aux;
		}
	}
	//se acabar o for, eh porque todos os nodos do cluster estao falhos
	free(cluster);
	return -2;
}


/*--------funcao que retorna os testes que serao executados pelo nodo--------*/
testsvector* testes(int n, int s, std:: vector<tnodo> &nodo) {
	//n = numero de nodos
	//s = log de n (numero de rodadas de teste [cada linha da tabela])

	int nodoTestador, position = 0;
	testsvector *tests = (testsvector*) malloc(sizeof(testsvector)*n);

	for(int k = 0; k < n; k++){
		tests[k].vector = (int*) malloc(sizeof(int)*n);//aloca o vetor com n posicoes
		tests[k].etapa = (int*) malloc(sizeof(int)*n);
		if(status(nodo[k].id) == 1) {
			tests[k].size = 0;
			continue;
		}//se for falho, nao faz a busca por testadores

		for(int i = 0; i < n; i++){ //for que percorre todos os nodos
			for(int j = 1; j <= s; j++){ //for para percorrer todas as etapas
				nodoTestador = findtester(i, j); //encontra testador do nodo

				//se o nodo for igual a k, entao adiciona a lista de testadores daquele nodo
				//significa que eh o testador que estamos procurando
				if(nodoTestador == k){
					// printf(">>>>> %d\n", j);
					tests[k].vector[position] = i;
					tests[k].etapa[position++] = j;
				}
			}
		}
		tests[k].size = position;
		position = 0;
	}
	return tests; //retorna o vetor com os testes executados
}

/*---------------inicio funcoes paxos-----------------*/
// o processo proposer recebe a resposta do acceptor contendo o numero da maior proposta
// recebida e tambem o valor (se for 0, é NULL, significa que ainda nao aceitou a proposta)
void send_prepareresponse(int acceptor, int proposer, int nproposta, int vproposta, int pos){
	printf("--------------------------------------\n");
    printf("O processo %d recebeu do processo %d o prepare response (numero %d, valor %d)\n",
    proposer, acceptor, nproposta, vproposta);
	printf(">>> A contagem de accepts esta em: %d\n", contresp[proposer].size());
	printf("--------------------------------------\n");
    // aqui o proposer comeca a execucao da fase 2 do algoritmo
    // o proposer deve verificar se recebeu a resposta de uma maioria de acceptors
    // printf("\n----------------------------------------\n");
	// printf("numeor proposta: %d\n", nproposta);
    // printf("recebeu %d accepts\n", contresp[nproposta].size());
    // // printf("prepare request enviados: %d\n", respostas[proposer][pos].first);
    // // printf("prepare response recebidos: %d\n", respostas[proposer][pos].second);
    // printf("----------------------------------------\n\n");
}

// essa funcao representa o processo acceptor na fase 1 do algoritmo
void send_preparerequest(int sender, int receiver, int numeroproposta){
	//sender = proposer e receiver = acceptor
    printf("O processo %d recebeu a proposta de numero %d do processo %d\n", receiver, numeroproposta, sender);
    // se a proposta recebida tem numero maior do que a maior ja recebida anteriormente,
    // entao responde ao prepare request com um prepare response contendo o numero n da maior
    // proposta recebida e a promessa de nao aceitar nenhuma proposta com numero menor
    if(numeroproposta > maiornumerorecebido[receiver].first){
        maiornumerorecebido[receiver].first = numeroproposta;
    }
}

// parametros: quem envia a mensagem, quem recebe, o numero da proposta e o valor dela
void send_acceptrequest(int sender, int receiver, int num, int val){
    printf("O processo %d enviou um accept request para o processo %d (numero %d,valor %d)\n", sender, receiver, num, val);
}

/*---------------fim funcoes paxos-----------------*/



/*------------funcoes do broadcast-----------------*/

//funcao para checar os acks
void checkacks(tnodo j, tmsg msg, int flag){
	bool r;
	//existe algum elemento em ack_set que respeite <j, *, m>?
	//retorna true, caso exista
	r = std::any_of(nodo[token].ack_set.begin(), nodo[token].ack_set.end(), [j, msg] (auto const &elem){
		return elem.idorigem == j.idr && !(elem.m.m.compare(msg.m));
	});
	//if ack_set intersec {<j, *, m>} = vazio
	if(!r){
		// printf("Acabaram os acks pendentes do processo\n");
		//veririca se source(m) esta sem-falha e tambem o j para mim (i)
		if(nodo[token].state[msg.idorigem] % 2 == 0 && nodo[token].state[j.id] % 2 == 0){
			// send_ack(msg, j); //enviando ack para pj
			ultimo_nodo = token;
			if(flag == 1)
				schedule(receive_ack, 1.0, j.idr);
			else if (flag == 2)
				schedule(receive_ack_acc, 1.0, j.idr);
		}
	}
}

void print_tests(){
	for(int j = 0; j < N; j++){
		for(int i = 0; i < tests[j].size; i++){
			printf("> %d (s: %d) ", tests[j].vector[i], tests[j].etapa[i]);
		}
		printf("\n");
	}
}


int freceive(int enviou, int recebeu){
	int k, etp, enviamsg, tam, cabou, result;
	bool res;
	enviamsg = 0;
	cabou = 0;

	if(nodo[recebeu].last[ult_prepare[enviou].idorigem].type == 'N'
	|| ult_prepare[enviou].timestamp > nodo[recebeu].last[ult_prepare[enviou].idorigem].timestamp){
		//last_i[source(m)] <- m
		//Atualiza o last do nodo atual com a mensagem que acabou
		//de ser recebida do outro nodo
		nodo[recebeu].last[ult_prepare[enviou].idorigem].type = ult_prepare[enviou].type;
		nodo[recebeu].last[ult_prepare[enviou].idorigem].m.assign(ult_prepare[enviou].m);
		nodo[recebeu].last[ult_prepare[enviou].idorigem].idorigem = ult_prepare[enviou].idorigem;
		nodo[recebeu].last[ult_prepare[enviou].idorigem].timestamp = ult_prepare[enviou].timestamp;
		nodo[recebeu].last[ult_prepare[enviou].idorigem].numproposta = ult_prepare[enviou].numproposta;
		nodo[recebeu].last[ult_prepare[enviou].idorigem].valproposta = ult_prepare[enviou].valproposta;

		ult_prepare[recebeu].type = ult_prepare[enviou].type;
		ult_prepare[recebeu].m.assign(ult_prepare[enviou].m);
		ult_prepare[recebeu].idorigem = ult_prepare[enviou].idorigem;
		ult_prepare[recebeu].timestamp = ult_prepare[enviou].timestamp;
		ult_prepare[recebeu].numproposta = ult_prepare[enviou].numproposta;
		ult_prepare[recebeu].valproposta = ult_prepare[enviou].valproposta;

		if(nodo[recebeu].last[ult_prepare[enviou].idorigem].type == 'P'){
			contresp[nodo[recebeu].last[ult_prepare[enviou].idorigem].numproposta].push_back(recebeu);
			tam = contresp[nodo[recebeu].last[ult_prepare[enviou].idorigem].numproposta].size();
			// printf("><><><><><><><><><><><><>< %d\n", contresp[nodo[recebeu].last[ult_prepare[enviou].idorigem].numproposta][tam-1]);
			//deliver(m)
			if(maiornumerorecebido[recebeu].first <= nodo[recebeu].last[ult_prepare[enviou].idorigem].numproposta){
				maiornumerorecebido[recebeu].first = nodo[recebeu].last[ult_prepare[enviou].idorigem].numproposta;
				npropostaatual = maiornumerorecebido[recebeu].first;



			}
			std::cout <<"[DELIVER] proposta \"" << nodo[recebeu].last[ult_prepare[enviou].idorigem].numproposta << "\" recebida do nodo " << enviou <<" foi entregue para a aplicacao pelo processo " << recebeu << " no tempo {" << time() << "}\n\n";
			if(tam > N/2){
				printf("[MAIORIA] O processo %d atingiu a maioria - recebeu %d accepts\n", ult_prepare[enviou].idorigem, tam);
				cabou = 1;
			}
		} else if(nodo[recebeu].last[ult_prepare[enviou].idorigem].type == 'R'){
			//deliver(m)
			//verifica:
			//maioria
			//se eh o maior numero recebido
			//se passou pela etapa 3
			if(contresp[nodo[recebeu].last[ult_prepare[enviou].idorigem].numproposta].size() >= N/2 && maiornumerorecebido[recebeu].first == nodo[recebeu].last[ult_prepare[enviou].idorigem].numproposta && passoupela3[nodo[recebeu].last[ult_prepare[enviou].idorigem].numproposta] == 1){
				std::cout <<"[DELIVER] proposta \"" << nodo[recebeu].last[ult_prepare[enviou].idorigem].numproposta << "\" e valor \"" << nodo[recebeu].last[ult_prepare[enviou].idorigem].valproposta << "\" recebida do nodo " << enviou <<" foi entregue para a aplicacao pelo processo " << recebeu << " no tempo {" << time() << "}\n\n";
				printf("[DECIDE] processo %d decide pelo valor %d\n", recebeu, nodo[recebeu].last[ult_prepare[enviou].idorigem].valproposta);
				printf("O processo %d atualizou seu registrador\n", recebeu);

				if(nodo[recebeu].reg == 0 && status(nodo[recebeu].id) == 0){
					nodo[recebeu].reg = nodo[recebeu].last[ult_prepare[enviou].idorigem].valproposta;
					for(int i = 0; i < N; i++){
						if(nodo[i].reg == 0 && status(nodo[i].id) == 0){
							printf("O processo atualiza o registrador do nodo: %d\n", i);
							nodo[i].reg = nodo[recebeu].last[ult_prepare[enviou].idorigem].valproposta;
						}
					}
					return 0;
				}
			}
		}

		// printf("Nodo: %d\n", nodo[ultima_msg->idorigem].idr);
		// printf("Status do nodo: %d\n", status(nodo[ultima_msg->idorigem].id));
		if(status(nodo[ult_prepare[enviou].idorigem].id) != 0){
			// std::cout <<"Fazendo novo broadcast para os vizinhos sem-falha... " << "\n";
			schedule(broadcast_prep, 1.0, recebeu);
			// broadcast(*ultima_msg);
			return 0;
		}
	}
	// encontrar identificador do cluster s de i que contem j
	// printf("****************** %d\n", tests[recebeu].size);
	for(int c = 0; c < tests[recebeu].size; c++){
		// printf("Agora vai calcular o cluster...\n");
		if(tests[recebeu].vector[c] == enviou){
			etp = tests[recebeu].etapa[c];
			// printf("Cluster do %d para o %d: %d\n", recebeu, enviou, etp);
			break;
		}
	}
	// print_tests();
	//retransmitir aos vizinhos corretos na arvore de source(m)
	//que pertencem ao cluster s que contém elementos
	//corretos de g

	for(int i = 0; i < N; i ++){
		for(int j = 0; j < tests[i].size; j++){
			for(int k = 1; k <= etp - 1; k++){
			// for(int k = etp - 1; k >= 1; k--){
				if(tests[i].vector[j] == recebeu && tests[i].etapa[j] == k && status(nodo[i].id) == 0){
					// existe algum elemento em ack_set que respeite <j, k, m>?
					//retorna true, caso exista
					res = std::any_of(nodo[recebeu].ack_set.begin(), nodo[recebeu].ack_set.end(), [i, enviou] (auto const &elem){
						return elem.idorigem == enviou && elem.iddestino == nodo[i].idr && !(elem.m.m.compare(ult_prepare[enviou].m));
					});
					//if <j,k,m> nao pertence ack_set(i)
					// j = enviou, k = ff_neighbor (aqui eh o nodo[i])
					if(!res){
						enviamsg = 1;
						// printf("---------- ETP = %d e K = %d\n", etp, k);
						nodo[recebeu].ack_set.insert(tset{enviou, nodo[i].idr, ult_prepare[enviou]});
						printf("[RBCAST] processo %d envia mensagem para processo %d\n", recebeu, nodo[i].idr);
						result = freceive(recebeu, nodo[i].idr);
						if(result == -1){
							// printf("Chegou no return aqui\n");
							return -1;
						}
					}
				}
			}
		}
	}
	checkacks(nodo[enviou], ult_prepare[enviou], 1);
	if(enviamsg == 0 && cabou == 1){
		//quando o processo nao envia mais mensagem para ninguem, eh porque eh folha
		//se o processo eh folha, precisa enviar para a origem a contagem de accepts
		// printf("##### %d nao envia mais msg\n", recebeu);
		printf("processo %d envia para %d a contagem de accepts\n", recebeu, nodo[recebeu].last[ult_prepare[enviou].idorigem].idorigem);
		send_prepareresponse(recebeu, nodo[recebeu].last[ult_prepare[enviou].idorigem].idorigem, maiornumerorecebido[recebeu].first, maiornumerorecebido[recebeu].second, contresp[nodo[recebeu].last[ult_prepare[enviou].idorigem].numproposta].size()-1);

		// respostas[token].size()-1
		// send_prepareresponse(i, token, maiornumerorecebido[i].first, maiornumerorecebido[i].second, pos)
		// for(int i  = 0; i < contresp[nodo[recebeu].last[ultima_msg->idorigem].numproposta].size(); i++)
		// 	printf("> %d", contresp[nodo[recebeu].last[ultima_msg->idorigem].numproposta][i]);
		// printf("\n");
		return -1;
	} else if (nodo[recebeu].last[ult_prepare[enviou].idorigem].type == 'R' && enviamsg == 0){
		printf("processo %d envia para %d a decisao\n", recebeu, nodo[recebeu].last[ult_prepare[enviou].idorigem].idorigem);
		send_prepareresponse(recebeu, nodo[recebeu].last[ult_prepare[enviou].idorigem].idorigem, maiornumerorecebido[recebeu].first, maiornumerorecebido[recebeu].second, contresp[nodo[recebeu].last[ult_prepare[enviou].idorigem].numproposta].size()-1);

	}else if (enviamsg == 0){
		printf("processo %d envia para %d a contagem de accepts\n", recebeu, nodo[recebeu].last[ult_prepare[enviou].idorigem].idorigem);
		send_prepareresponse(recebeu, nodo[recebeu].last[ult_prepare[enviou].idorigem].idorigem, maiornumerorecebido[recebeu].first, maiornumerorecebido[recebeu].second, contresp[nodo[recebeu].last[ult_prepare[enviou].idorigem].numproposta].size()-1);

		// for(int i  = 0; i < contresp[nodo[recebeu].last[ultima_msg->idorigem].numproposta].size(); i++)
		// 	printf("> %d", contresp[nodo[recebeu].last[ultima_msg->idorigem].numproposta][i]);
		// printf("\n");
	}

	return 0;
}


int freceive_acc(int enviou, int recebeu){
	int k, etp, enviamsg, tam, cabou, result;
	bool res;
	enviamsg = 0;
	cabou = 0;
	// printf("<----Ultimo nodo - chegou na receive: %d\n", enviou);
	// printf("TYPE = %c\n", nodo[token].last[ultima_msg->idorigem].type);
	// std::cout << "-----> schedule:" << ultima_msg->m << "\n";
	// printf("ULTIMO NODO: %d TOKEN: %d\n", enviou, token);
	if(nodo[recebeu].last[ult_accept[enviou].idorigem].type == 'N'
	|| ult_accept[enviou].timestamp > nodo[recebeu].last[ult_accept[enviou].idorigem].timestamp){
		//last_i[source(m)] <- m
		//Atualiza o last do nodo atual com a mensagem que acabou
		//de ser recebida do outro nodo
		nodo[recebeu].last[ult_accept[enviou].idorigem].type = ult_accept[enviou].type;
		nodo[recebeu].last[ult_accept[enviou].idorigem].m.assign(ult_accept[enviou].m);
		nodo[recebeu].last[ult_accept[enviou].idorigem].idorigem = ult_accept[enviou].idorigem;
		nodo[recebeu].last[ult_accept[enviou].idorigem].timestamp = ult_accept[enviou].timestamp;
		nodo[recebeu].last[ult_accept[enviou].idorigem].numproposta = ult_accept[enviou].numproposta;
		nodo[recebeu].last[ult_accept[enviou].idorigem].valproposta = ult_accept[enviou].valproposta;

		ult_accept[recebeu].type = ult_accept[enviou].type;
		ult_accept[recebeu].m.assign(ult_accept[enviou].m);
		ult_accept[recebeu].idorigem = ult_accept[enviou].idorigem;
		ult_accept[recebeu].timestamp = ult_accept[enviou].timestamp;
		ult_accept[recebeu].numproposta = ult_accept[enviou].numproposta;
		ult_accept[recebeu].valproposta = ult_accept[enviou].valproposta;

		if(nodo[recebeu].last[ult_accept[enviou].idorigem].type == 'P'){
			contresp[nodo[recebeu].last[ult_accept[enviou].idorigem].numproposta].push_back(recebeu);
			tam = contresp[nodo[recebeu].last[ult_accept[enviou].idorigem].numproposta].size();
			// printf("><><><><><><><><><><><><>< %d\n", contresp[nodo[recebeu].last[ult_accept[enviou].idorigem].numproposta][tam-1]);
			//deliver(m)
			if(maiornumerorecebido[recebeu].first <= nodo[recebeu].last[ult_accept[enviou].idorigem].numproposta){
				maiornumerorecebido[recebeu].first = nodo[recebeu].last[ult_accept[enviou].idorigem].numproposta;
				npropostaatual = maiornumerorecebido[recebeu].first;



			}
			std::cout <<"[DELIVER] proposta \"" << nodo[recebeu].last[ult_accept[enviou].idorigem].numproposta << "\" recebida do nodo " << enviou <<" foi entregue para a aplicacao pelo processo " << recebeu << " no tempo {" << time() << "}\n\n";
			if(tam > N/2){
				printf("[MAIORIA] O processo %d atingiu a maioria - recebeu %d accepts\n", ult_accept[enviou].idorigem, tam);
				cabou = 1;
			}
		} else if(nodo[recebeu].last[ult_accept[enviou].idorigem].type == 'R'){
			//deliver(m)
			//verifica:
			//maioria
			//se eh o maior numero recebido
			//se passou pela etapa 3
			if(contresp[nodo[recebeu].last[ult_accept[enviou].idorigem].numproposta].size() >= N/2 && maiornumerorecebido[recebeu].first == nodo[recebeu].last[ult_accept[enviou].idorigem].numproposta && passoupela3[nodo[recebeu].last[ult_accept[enviou].idorigem].numproposta] == 1){
				std::cout <<"[DELIVER] proposta \"" << nodo[recebeu].last[ult_accept[enviou].idorigem].numproposta << "\" e valor \"" << nodo[recebeu].last[ult_accept[enviou].idorigem].valproposta << "\" recebida do nodo " << enviou <<" foi entregue para a aplicacao pelo processo " << recebeu << " no tempo {" << time() << "}\n\n";
				printf("[DECIDE] processo %d decide pelo valor %d\n", recebeu, nodo[recebeu].last[ult_accept[enviou].idorigem].valproposta);
				printf("O processo %d atualizou seu registrador\n", recebeu);

				if(nodo[recebeu].reg == 0 && status(nodo[recebeu].id) == 0){
					nodo[recebeu].reg = nodo[recebeu].last[ult_accept[enviou].idorigem].valproposta;
					for(int i = 0; i < N; i++){
						if(nodo[i].reg == 0 && status(nodo[i].id) == 0){
							printf("O processo atualiza o registrador do nodo: %d\n", i);
							nodo[i].reg = nodo[recebeu].last[ult_accept[enviou].idorigem].valproposta;
						}
					}
					return 0;
				}
			}
		}

		// printf("Nodo: %d\n", nodo[ult_accept[enviou].idorigem].idr);
		// printf("Status do nodo: %d\n", status(nodo[ultima_msg->idorigem].id));
		if(status(nodo[ult_accept[enviou].idorigem].id) != 0){
			// std::cout <<"Fazendo novo broadcast para os vizinhos sem-falha... " << "\n";
			schedule(broadcast_acc, 1.0, recebeu);
			// broadcast(*ultima_msg);
			return 0;
		}
	}
	// encontrar identificador do cluster s de i que contem j
	// printf("****************** %d\n", tests[recebeu].size);
	for(int c = 0; c < tests[recebeu].size; c++){
		// printf("Agora vai calcular o cluster...\n");
		if(tests[recebeu].vector[c] == enviou){
			etp = tests[recebeu].etapa[c];
			// printf("Cluster do %d para o %d: %d\n", recebeu, enviou, etp);
			break;
		}
	}
	// print_tests();
	//retransmitir aos vizinhos corretos na arvore de source(m)
	//que pertencem ao cluster s que contém elementos
	//corretos de g

	for(int i = 0; i < N; i ++){
		for(int j = 0; j < tests[i].size; j++){
			for(int k = 1; k <= etp - 1; k++){
			// for(int k = etp - 1; k >= 1; k--){
				if(tests[i].vector[j] == recebeu && tests[i].etapa[j] == k && status(nodo[i].id) == 0){
					// existe algum elemento em ack_set que respeite <j, k, m>?
					//retorna true, caso exista
					res = std::any_of(nodo[recebeu].ack_set.begin(), nodo[recebeu].ack_set.end(), [i, enviou] (auto const &elem){
						return elem.idorigem == enviou && elem.iddestino == nodo[i].idr && !(elem.m.m.compare(ult_accept[enviou].m));
					});
					//if <j,k,m> nao pertence ack_set(i)
					// j = enviou, k = ff_neighbor (aqui eh o nodo[i])
					if(!res){
						enviamsg = 1;
						// printf("---------- ETP = %d e K = %d\n", etp, k);
						nodo[recebeu].ack_set.insert(tset{enviou, nodo[i].idr, ult_accept[enviou]});
						printf("[RBCAST] processo %d envia mensagem para processo %d\n", recebeu, nodo[i].idr);
						result = freceive(recebeu, nodo[i].idr);
						if(result == -1){
							// printf("Chegou no return aqui\n");
							return -1;
						}
					}
				}
			}
		}
	}
	checkacks(nodo[enviou], ult_accept[enviou], 2);
	if(enviamsg == 0 && cabou == 1){
		//quando o processo nao envia mais mensagem para ninguem, eh porque eh folha
		//se o processo eh folha, precisa enviar para a origem a contagem de accepts
		// printf("##### %d nao envia mais msg\n", recebeu);
		printf("processo %d envia para %d a contagem de accepts\n", recebeu, nodo[recebeu].last[ult_accept[enviou].idorigem].idorigem);
		send_prepareresponse(recebeu, nodo[recebeu].last[ult_accept[enviou].idorigem].idorigem, maiornumerorecebido[recebeu].first, maiornumerorecebido[recebeu].second, contresp[nodo[recebeu].last[ult_accept[enviou].idorigem].numproposta].size()-1);

		// respostas[token].size()-1
		// send_prepareresponse(i, token, maiornumerorecebido[i].first, maiornumerorecebido[i].second, pos)
		// for(int i  = 0; i < contresp[nodo[recebeu].last[ult_accept[enviou].idorigem].numproposta].size(); i++)
		// 	printf("> %d", contresp[nodo[recebeu].last[ult_accept[enviou].idorigem].numproposta][i]);
		// printf("\n");
		return -1;
	} else if (nodo[recebeu].last[ult_accept[enviou].idorigem].type == 'R' && enviamsg == 0){
		printf("processo %d envia para %d a decisao\n", recebeu, nodo[recebeu].last[ult_accept[enviou].idorigem].idorigem);
		send_prepareresponse(recebeu, nodo[recebeu].last[ult_accept[enviou].idorigem].idorigem, maiornumerorecebido[recebeu].first, maiornumerorecebido[recebeu].second, contresp[nodo[recebeu].last[ult_accept[enviou].idorigem].numproposta].size()-1);

	}else if (enviamsg == 0){
		printf("processo %d envia para %d a contagem de accepts\n", recebeu, nodo[recebeu].last[ult_accept[enviou].idorigem].idorigem);
		send_prepareresponse(recebeu, nodo[recebeu].last[ult_accept[enviou].idorigem].idorigem, maiornumerorecebido[recebeu].first, maiornumerorecebido[recebeu].second, contresp[nodo[recebeu].last[ult_accept[enviou].idorigem].numproposta].size()-1);

		// for(int i  = 0; i < contresp[nodo[recebeu].last[ult_accept[enviou].idorigem].numproposta].size(); i++)
		// 	printf("> %d", contresp[nodo[recebeu].last[ult_accept[enviou].idorigem].numproposta][i]);
		// printf("\n");
	}

	return 0;
}


//funcao chamada quando o processo i detecta o j como falho
void crash(tnodo j){
	int etp = 0, k = 0;
	bool r;
	//IF remover acks pendentes para <j,*,*>
	//ELSE enviar m para vizinho k, se k existir
	for(std:: set<tset>::iterator cont = nodo[token].ack_set.begin(); cont != nodo[token].ack_set.end(); cont++){
		if(cont->idorigem == j.idr)
			nodo[token].ack_set.erase(cont);
		else if(cont->iddestino == j.idr){
			//encontrar identificador do cluster s de i que contem j
			for(int c = 0; c < tests[token].size; c++)
				if(tests[token].vector[c] == j.idr){
					etp = tests[token].etapa[c];
					break;
				}
			for(std:: set<tset>::iterator cont2 = nodo[token].ack_set.begin(); cont2 != nodo[token].ack_set.end(); cont2++){
				//Se a mensagem eh a mesma que estamos analisando no for mais externo
				//se o nodo destino dessa mensagem achada esta sem-falha
				//e se esse nodo esta no mesmo cluster (s) que o nodo que falhou estava no nodo i
				if(!(cont2->m.m.compare(cont->m.m)) && status(nodo[cont2->iddestino].id) == 0 && tests[token].etapa[cont2->iddestino] == etp){
					//k <- ff_neighbor_i(s)
					for(int i = 0; i < tests[token].size; i++){
						if(tests[token].etapa[i] == etp && status(nodo[tests[token].vector[i]].id) == 0){
							k = tests[token].vector[i];
							// printf("Encontrou o ff_neighbor, que eh: %d", k);
							break;
						}
					}
					// printf("Vai verificar se alguem mandou mensagem para o ff_neighbor\n");
					//existe algum elemento em ack_set que respeite <*, k, m>?
					//retorna true, caso exista
					// --- cont eh do ..primeiro for, que esta passando por todos os elementos do ack_set
					// --- k eh o primeiro nodo sem falha obtido pela ff_neighbor
					r = std::any_of(nodo[token].ack_set.begin(), nodo[token].ack_set.end(), [cont, k] (auto const &elem){
						return elem.idorigem == cont->idorigem && elem.iddestino == k && !(elem.m.m.compare(cont->m.m));
					});
					//se nao encontrou o elemento, adiciona
					if(!r){
						// printf("Ninguem mandou :( vamos mandar agora\n");
						nodo[token].ack_set.insert(tset{cont->idorigem, k, cont->m});
						// send_msg(cont->m.m, cont2->iddestino, nodo[k]);
						//VERIFICAR MUUUUUUUUUUUITO ESSA FUNCAO HAHAHAH
						printf("--> Enviando mensagem do nodo %d para o nodo %d apos detectar %d falho\n", token, k, j.idr);
						freceive(token, k);
					}
				}
			}
		}
	}
	//fora do for
	//garantir a entrega da mensagem mais atual a todos os processos corretos
	//em dest(m) da ultima mensagem recebida de j (processo que falhou)
	//if last(i)[j] != vazio
	// printf("Chega aqui, pelo menos\n");
	// std::cout << nodo[token].last[j.idr].type << "\n";
	if(nodo[token].last[j.idr].type != 'N'){
		// printf("Sera que entra no if?\n");
		//existe algum elemento em ack_set que respeite <*, i, m>?
		//retorna true, caso exista
		//--- existe no ack_set de j (nodo falho) o registro de que eu (i) sou
		//um destinatario da ultima mensagem que eu recebi de j?
		//i E dest(last[j])
		r = std::any_of(j.ack_set.begin(), j.ack_set.end(), [j] (auto const &elem){
			return elem.iddestino == token && !(elem.m.m.compare(nodo[token].last[j.idr].m));
		});
		if(r){
			// printf("Aqui ele deve enviar a mensagem pro proximo do cluster\n");
			// broadcast(nodo[token].last[j.idr]);

			if(nodo[token].last[j.idr].type == 'P'){
				ult_prepare[token].type = nodo[token].last[j.idr].type;
				ult_prepare[token].m.assign(nodo[token].last[j.idr].m);
				ult_prepare[token].idorigem = nodo[token].last[j.idr].idorigem;
				ult_prepare[token].timestamp = nodo[token].last[j.idr].timestamp;
				ult_prepare[token].numproposta = nodo[token].last[j.idr].numproposta;
				ult_prepare[token].valproposta = nodo[token].last[j.idr].valproposta;
				// printf("Broadcast do crash\n");
				schedule(broadcast_prep, 2.0, token);
			}else{
				ult_accept[token].type = nodo[token].last[j.idr].type;
				ult_accept[token].m.assign(nodo[token].last[j.idr].m);
				ult_accept[token].idorigem = nodo[token].last[j.idr].idorigem;
				ult_accept[token].timestamp = nodo[token].last[j.idr].timestamp;
				ult_accept[token].numproposta = nodo[token].last[j.idr].numproposta;
				ult_accept[token].valproposta = nodo[token].last[j.idr].valproposta;
				// printf("Broadcast do crash\n");
				schedule(broadcast_acc, 2.0, token);
			}

		}
	}
}

/*---------------fim funcoes broadcast-----------------*/

//funcao que printa o vetor state de todos os nodos
void print_state(std:: vector<tnodo> &nodo, int n){

	// for(int i = 0; i < n; i++)
	// 	if(status(nodo[i].id) == 0)
	// 		printf("nodo sem falha %d\n", i);


	printf ("\nVetor STATE(i): ");
	for (int i = 0; i < n; i++)
		printf ("%d  ", i);
	printf ("\n");
	printf ("-------------------------------------\n");

	for (int i = 0; i < n; i++){
		if(status(nodo[i].id) == 0){
			printf (">     Nodo %d | ", i);
			for (int j = 0; j < n; j++)
					if(j < 9)
						printf (" %d ", nodo[i].state[j]);
					else
						printf ("  %d ", nodo[i].state[j]);
			printf("\n");
		}
	}
	printf("\n");
}

void print_init(){
	printf ("===========================================================\n");
	printf ("         Execucao do algoritmo Paxos + V-Cube\n");
	printf ("         Aluna: Acacia dos Campos da Terra\n");
	printf ("         Professor: Elias P. Duarte Jr.\n");
	printf ("===========================================================\n\n");
}

void print_end(int r, int n){
	//apos o fim do tempo, printa os vetores states
	printf("\n--------------------------------------------------------------\n");
	printf("                       RESULTADOS\n");
	printf("\nNúmero de rodadas total do programa: %d\n", r);
	printf("\nVetor STATE ao final do diagnostico:\n");
	print_state(nodo, n);
}


int main(int argc, char const *argv[]) {
	static int event, r, i, aux, logtype, etp, k, x, pos;
	static int nodosemfalha = 0, nodofalho = 0, nrodadas = 0, ntestes = 0, totalrodadas = 0;
	static char fa_name[5];	//facility representa o objeto simulado
	bool res, resb = true;
	int idx = -1;

//-------variáveis dos eventos passados por arquivo---------
	char evento[7];
	int processo;
	float tempo;

	 if(argc != 3){
		puts("Uso correto: ./vcube <arquivo> -parametro (-r ou -c)");
		exit(1);
	}

	//logtype, if 0 - reduced, if 1 - complete
	if(strcmp("-c", argv[2]) == 0)
		logtype = 1;
	else if(strcmp("-r", argv[2]) == 0)
		logtype = 0;
	else{
		printf("Parametro incorreto (-c ou -r)\n");
		exit(1);
	}

	//o arquivo foi chamado de tp por nenhum motivo especifico
	//faz a leitura do numero de nodos
	FILE *tp = fopen(argv[1], "r");
	if (tp != NULL)
		fscanf(tp, "%d\n", &N);
	else
		printf("Erro ao ler arquivo\n");
	fclose(tp);

	smpl(0, "programa hyperpaxos");
	reset();
	stream(1);
	// nodo = (tnodo*) malloc(sizeof(tnodo)*N);
	nodo.resize(N);

	nodo.resize(N);
    maiornumerorecebido.resize(N);
    passoupela3.resize(50);
	contresp.resize(50);//cada posicao representa uma proposta

	//aqui foram inseridas 50 propostas de valores aleatorios
    //os numeros de propostas sao definidos de acordo com o processo
    for(int i = 0; i < 50; i++){
        propostas.push_back(std::make_pair(i, rand()%200));
    }
    quemrespondeu.resize(N);
    numeros.resize(N);
    respostas.resize(N);
	ult_accept.resize(N);
	ult_prepare.resize(N);

    std::fill(numeros.begin(), numeros.end(), -1);

	for (i = 0; i < N; i++) {
	 	memset(fa_name, '\0', 5);
		// 	printf(fa_name, "%d", i);
	 	nodo[i].id = facility(fa_name, 1);
		nodo[i].idr = i;

		nodo[i].papel.resize(1);
        nodo[i].papel[0] = 2;
        nodo[i].reg = 0;

		//para cada nodo cria e inicializa vetor state com -1(UNKNOWN)
		nodo[i].state = (int*) malloc (sizeof(int)*N);
		//para cada nodo cria vetor last
		nodo[i].last.resize(N);
		for (int j = 0; j < N; j++){
			nodo[i].state[j] = 0;
			nodo[i].last[j].type = 'N';
			nodo[i].last[j].idorigem = -1;
			nodo[i].last[j].timestamp = -1;
		}
	 }
	//  nodo[2].papel[0] = 1;
	 print_init();
     printf("Numero de nodos no sistema: %d\n", N);
	 /*schedule inicial*/
	 for (i = 0; i < N; i++)
	 	schedule(test, 30.0, i);

	tp = fopen(argv[1], "r");
	fscanf(tp, "%d\n", &N);
	while(!feof(tp)){
		fscanf(tp, "%s %f %d\n", evento, &tempo, &processo);
		// printf("%s %f %d\n", evento, tempo, processo);
		schedule(strcmp("propose", evento) == 0 ? propose : (strcmp("fault", evento) == 0 ? fault : (strcmp("repair", evento) == 0 ? repair : (strcmp("broadcast", evento) == 0 ? broadcast_prep : (strcmp("new_msg", evento) == 0 ? new_msg : test)))), tempo, processo);
		//escalona os eventos. Faz a verificação de string pois o schedule não aceita string como parâmetro
	}
	fclose(tp);

	aux = N;
	//  printf("Programa inicializa - todos os nodos estao *sem-falha*\n");
	 for(s = 0; aux != 1; aux /= 2, s++);//for para calcular o valor de S (log de n)
	 tests = testes(N, s, nodo); //calcula quais os testes executados
	//  for(i = 0; i < N; i++){//printa os testes executados por cada nodo, apos ser calculado
	// 	 printf("O nodo %d testa os nodos: ", i);
	// 	 for(int j = 0; j < tests[i].size; j++)
	// 		 printf("%d ", tests[i].vector[j]);
	// 	 printf("\n");
	//  }



	// schedule(broadcast, 1.0, token);

	 while(time() < 250.0) {
	 	cause(&event, &token); //causa o proximo evento
	 	switch(event) {
	 		case test:
	 		if(status(nodo[token].id) != 0){
				// nodofalho++;  //contabiliza nodos FALHOS
				break; //se o nodo for falho, então quebra
			}else{
				nodosemfalha++;  //contabiliza nodos SEM-FALHA
			}

			//se o state de token for um numero impar, esta desatualizado
			//esta marcando como falho (impar = falho, par = sem-falha)
			//acrescenta-se +1, para indicar que esta sem-falha e ficar certo
			if((nodo[token].state[token] % 2) != 0)
				nodo[token].state[token]++;
			// printf("[%5.1f] ", time());
			// printf("Nodo %d testa: ", token);
			for(int i = 0; i < tests[token].size; i++){
				// printf("%d ", tests[token].vector[i]);
				if(status(nodo[tests[token].vector[i]].id) == 0) {//se o nodo estiver sem-falha
					ntestes++;  //contabiliza os testes realizados

					//atualiza o valor no state, caso esteja desatualizado
					if((nodo[token].state[tests[token].vector[i]] % 2) != 0){
						if(nodo[token].state[tests[token].vector[i]] != -1)
							// printf("(nodo detecta evento: nodo %d sem-falha) ", tests[token].vector[i]);
						nodo[token].state[tests[token].vector[i]]++;
					}

					for(int j = 0; j < N; j++){//for para verificar as novidades
						if(nodo[token].state[j] == -1){
							//caso seja o inicio do programa, atualiza o state com 0
							nodo[token].state[j] = 0;
						}else if(nodo[token].state[j] < nodo[tests[token].vector[i]].state[j]){
							//caso nao seja o inicio e o valor do state do token seja menor
							//que o do state de j, entao copia as novidades
							nodo[token].state[j] = nodo[tests[token].vector[i]].state[j];
							// printf("(nodo %d obtem info nodo %d: nodo %d falho) ", token, tests[token].vector[i], j);

							crash(nodo[j]);
						}
					}
				} else if((nodo[token].state[tests[token].vector[i]] % 2) == 0) {
					//caso o nodo esteja falho e o state esteja desatualizado
					//ou seja, esta como nao falho, mas na verdade esta falho
					//entao atualiza o vetor state
					nodo[token].state[tests[token].vector[i]]++;
					// printf("(nodo detecta evento: nodo %d falho) ", tests[token].vector[i]);

					// printf("------- %d\n", nodo[l].idr);
					//envia o nodo que falhou
					crash(nodo[tests[token].vector[i]]);
				}
			}
			// printf("\n");


			// printf("\n\t\t>>> nodo falho: %d nodo sem falha: %d\n\n", nodofalho, nodosemfalha);
// -------------------------------verificacao para numero de rodadas-------------------------------------------------------------
			if((nodofalho + nodosemfalha == N) && time() > 30.0){  //so entra se todos foram testados
				int nodosf, i, end = 1;
				nodosemfalha = 0;

				for (nodosf = 0; nodosf < N; nodosf++){  //encontra o primeiro nodo SEM-FALHA
					if (status(nodo[nodosf].id) == 0)
						break;
				}

				if (nodosf != N-1){ //verifica se nao eh o ultimo, para evitar seg fault
					for (i = nodosf + 1; i < N; i++){ //compara o vetor de nodosf com os demais
						if (status(nodo[i].id) != 0)//se for nodo FALHO apenas passa para o proximo
							continue;

						for (int j = 0; j < N; j++){  //compara se state dos SEM-FALHA sao iguais
							if (nodo[nodosf].state[j] != nodo[i].state[j])
								end = 0;//significa que os vetores estao diferentes
						}
					}
				}
				nrodadas++;//aumenta o numero de rodadas, independente dos vetores estarem iguais
				totalrodadas++;//aumenta as rodadas do programa todo
				if(logtype)
					print_state(nodo, N);
				// printf("\t------ Fim da rodada %d ------\n", totalrodadas);

				if (i == N && end == 1){
					//todos os vetores SEM-FALHA foram comparados e sao iguais
					//entao o evento foi diagnosticado, printa a quantidade de rodadas e testes necessarios
					// printf ("O evento precisou de %d rodadas e %d testes para ser diagnosticado\n", nrodadas, ntestes);
				}
				// print_tests();
			}

// ---------------------------------------fim da verificacao de rodadas-----------------------------------------------------

	 		schedule(test, 30.0, token);
	 		break;

	 		case fault:
			// print_state(nodo, N);
			//atualiza os valores para continuar contando
			nodosemfalha = 0;
			nodofalho++;
			nrodadas = 0;
			ntestes = 0;
	 		r = request(nodo[token].id, token, 0);
	 		if(r != 0) {
	 			puts("Impossível falhar nodo");
	 			exit(1);
	 		}
				printf("EVENTO: O nodo %d falhou no tempo %5.1f\n", token, time());

//-----------------------a cada evento, recalcula o vetor tests----------------------------------
			free(tests);
			tests = testes(N, s, nodo); //calcula quais os testes executados
			// for(i = 0; i < N; i++){//printa os testes executados por cada nodo, apos ser calculado
			// 	printf("O nodo %d testa os nodos: ", i);
			// 	for(int j = 0; j < tests[i].size; j++)
			// 		printf("%d ", tests[i].vector[j]);
			// 	printf("\n");
			// }
			break;

	 		case repair:
			// print_state(nodo, N);
			//atualiza os valores para continuar contando
			nodofalho--; //se recuperou, tem um nodo falho a menos
			nodosemfalha = 0;
			nrodadas = 0;
			ntestes = 0;

	 		release(nodo[token].id, token);
	 		printf("EVENTO: O nodo %d recuperou no tempo %5.1f\n", token, time());

//-----------------------a cada evento, recalcula o vetor tests----------------------------------
			free(tests);
			tests = testes(N, s, nodo); //calcula quais os testes executados
			// for(i = 0; i < N; i++){//printa os testes executados por cada nodo, apos ser calculado
			// 	printf("O nodo %d testa os nodos: ", i);
			// 	for(int j = 0; j < tests[i].size; j++)
			// 		printf("%d ", tests[i].vector[j]);
			// 	printf("\n");
			// }
			schedule(test, 30.0, token);
			break;

			case propose:
			int c;
			c = 0;
            // aqui o nodo token (proposer coordenador) precisa saber quais processos sao acceptors,
            // para enviar o prepare request para uma maioria de acceptors
            // precisa tambem selecionar uma proposta N, para isso sera mantido um vetor de propostas

            //armazena quantas vezes esse processo ja fez propose
            //para calcular qual o numero do propose (nunca é o mesmo)
			for(int i = 0; i < N; i++){
				if(status(nodo[i].id) == 0){
					c++;
				}
			}
			if(c <= N/2){
				printf("SORRY! O consenso nao eh possivel, pois ha pelo menos n/2 - 1 nodos falhos\n");
				break;
			}
            numeros[token]++;

            printf("[EVENTO] O processo %d esta fazendo o propose! (tempo %5.1f)\n\n", token, time());
            respostas[token].push_back(std:: make_pair(0,0));
            pos = respostas[token].size()-1;
            schedule(etapa1, 5.0, token);
            break;

            case etapa1:
            //apaga todas as respostas anteriores, para caso receba uma nova proposta
            quemrespondeu[token].erase(quemrespondeu[token].begin(), quemrespondeu[token].end());


			//gerando mensagem a ser enviada
			int tam;
			mensagens.push_back(tmsg{'P', "teste "+ std::to_string(timestamp), nodo[token].idr, timestamp, propostas[token + N * numeros[token]].first, -1});
			tam = mensagens.size()-1;
			// std::cout << "Mensagem armazenada::::::::::::::::::" << mensagens[mensagens.size()-1].m << "\n";

			ult_prepare[token].type = mensagens[tam].type;
			ult_prepare[token].m.assign(mensagens[tam].m);
			ult_prepare[token].idorigem = mensagens[tam].idorigem;
			ult_prepare[token].timestamp = mensagens[tam].timestamp;
			ult_prepare[token].numproposta = mensagens[tam].numproposta;
			ult_prepare[token].valproposta = mensagens[tam].valproposta;
			//incrementa o timestamp sempre que gera uma nova mensagem
			timestamp++;

			schedule(broadcast_prep, 1.0, token);

			//envia para os processos acceptors o numero da proposta (first no pair)
            for(int i = 0; i < N; i ++){
                if(nodo[i].papel[0] == 2 && status(nodo[i].id) == 0){
                    enviados++;
                    respostas[token][pos].first++;
                    // send_preparerequest(token, i, propostas[token + N * numeros[token]].first);
                }
            }
            schedule(etapa2, 5.0, token);
            break;

            case etapa2:
            for(int i = 0; i < N; i ++){
                if(nodo[i].papel[0] == 2 && status(nodo[i].id) == 0){
                    if(propostas[token + N * numeros[token]].first >= maiornumerorecebido[i].first){
                        maiornumerorecebido[i].first = propostas[token + N * numeros[token]].first;
                        // printf(">>>>>>>>>>>>>>>>>>> %d e %d\n", propostas[token + N * numeros[token]].first, propostas[token + N * numeros[token]].second);
                        // responsesrecebidas[sender]++;
                        respostas[token][pos].second++;
                        // aqui: quem respondeu a mensagem do token foi o processo i
                        quemrespondeu[token].push_back(i);
                         npropostaatual = maiornumerorecebido[i].first;
                        // send_prepareresponse(i, token, maiornumerorecebido[i].first, maiornumerorecebido[i].second, pos);
                    }
                }
            }

            schedule(etapa3, 15.0, token);
            break;

            case etapa3:

            // verifica se recebeu a maioria das respostas para aquela proposta
            // no caso, a proposta 1, porque esse trabalho esta ficando simples demais
            // printf("maiornumerorecebido: %d >>>>>> token + N * numeros[token]: %d\n", maiornumerorecebido[token].first, (token + N * numeros[token]));
            if(respostas[token][pos].second >= (respostas[token][pos].first / 2) && maiornumerorecebido[token].first == token + N * numeros[token]){
                passoupela3[token + N * numeros[token]] = 1;
                // printf(">>> Recebeu resposta da maioria!!\n");

				mensagens.push_back(tmsg{'R', "teste "+ std::to_string(timestamp), nodo[token].idr, timestamp, propostas[token + N * numeros[token]].first, propostas[token + N * numeros[token]].second});
				tam = mensagens.size()-1;
				// std::cout << "Mensagem armazenada::::::::::::::::::" << mensagens[mensagens.size()-1].m << "\n";

				ult_accept[token].type = mensagens[tam].type;
				ult_accept[token].m.assign(mensagens[tam].m);
				ult_accept[token].idorigem = mensagens[tam].idorigem;
				ult_accept[token].timestamp = mensagens[tam].timestamp;
				ult_accept[token].numproposta = mensagens[tam].numproposta;
				ult_accept[token].valproposta = mensagens[tam].valproposta;
				//incrementa o timestamp sempre que gera uma nova mensagem
				timestamp++;

				schedule(broadcast_acc, 1.0, token);
            }

			// agora o proposer envia o accept request para os processos dos quais recebeu a resposta
            schedule(etapa4, 5.0, token);
            break;

            case etapa4:

            if(respostas[token][pos].second >= (respostas[token][pos].first / 2) && maiornumerorecebido[token].first == token + N * numeros[token] && passoupela3[token + N * numeros[token]] == 1){
                for(int i = 0; i < quemrespondeu[token].size(); i++){
                    if(status(nodo[quemrespondeu[token][i]].id) == 0 && nodo[quemrespondeu[token][i]].reg == 0){
                        // nodo[quemrespondeu[token][i]].reg = propostas[token + N * numeros[token]].second;
                        // printf("processo %d decidiu pelo valor %d\n", quemrespondeu[token][i], propostas[token + N * numeros[token]].second);
                        // printf("O processo %d atualizou seu proprio registrador com o valor %d\n", nodo[quemrespondeu[token][i]].idr, propostas[token + N * numeros[token]].second);
                        for(int i = 0; i < N; i++){
                            maiornumerorecebido[i].second = propostas[token + N * numeros[token]].second;
                            if(status(nodo[i].id) == 0 && nodo[i].reg == 0){
                                // nodo[i].reg = propostas[token + N * numeros[token]].second;
                                // printf("O processo %d aprendeu o valor %d e atualizou seu registrador\n", nodo[i].idr, propostas[token + N * numeros[token]].second);
                            }
                        }
                    }
                }
            }

            break;

			case receive_ack:
				// x | <x,j,m> pertence ack_set(i)
				for(std:: set<tset>::iterator cont = nodo[token].ack_set.begin(); cont != nodo[token].ack_set.end(); cont++){
					// std::cout << "Origem: " << cont->idorigem << " Destino: " << cont->iddestino << "\n";
					if(cont->iddestino == ultimo_nodo && !(cont->m.m.compare(ult_prepare[token].m))){
						idx = cont->idorigem;
					}
				}
				//if meu id != x, identifica nodo e chama checkacks
				if(idx != -1 && idx != nodo[token].idr){
					// printf("%d\n", idx);
					checkacks(nodo[idx], ult_prepare[token], 1);
				}
				break;

				case receive_ack_acc:
					// x | <x,j,m> pertence ack_set(i)
					for(std:: set<tset>::iterator cont = nodo[token].ack_set.begin(); cont != nodo[token].ack_set.end(); cont++){
						// std::cout << "Origem: " << cont->idorigem << " Destino: " << cont->iddestino << "\n";
						if(cont->iddestino == ultimo_nodo && !(cont->m.m.compare(ult_accept[token].m))){
							idx = cont->idorigem;
						}
					}
					//if meu id != x, identifica nodo e chama checkacks
					if(idx != -1 && idx != nodo[token].idr){
						// printf("%d\n", idx);
						checkacks(nodo[idx], ult_accept[token], 2);
					}
					break;


			case new_msg:
			// int tam;
				//armazena em mensagens todas as mensagens ja enviadas pelo broadcast,
				//contendo o id de origem e o timestamp (unico para cada mensagem)
				//--- transforma a string em um tmsg ---
				mensagens.push_back(tmsg{'T', "teste "+ std::to_string(timestamp), nodo[token].idr, timestamp});
				tam = mensagens.size()-1;
				// std::cout << "Mensagem armazenada::::::::::::::::::" << mensagens[mensagens.size()-1].m << "\n";

				//incrementa o timestamp sempre que gera uma nova mensagem
				timestamp++;
				break;


			case broadcast_prep:
			//if source(m) == i
			int result;
			bool res2, res3;
			result = 0;
			if(ult_prepare[token].idorigem == token && status(nodo[token].id) == 0){
				//isso aqui não vai gerar look infinito não? HAHAHAHAHAHAHAHAHAH
				while(resb){
					//existe algum elemento em ack_set que respeite <i, *, last_i[i]>?
					//retorna true, caso exista
					resb = std::any_of(nodo[token].ack_set.begin(), nodo[token].ack_set.end(), [] (auto const &elem){
						return elem.idorigem == token && !(elem.m.m.compare(nodo[token].last[token].m)) && elem.m.idorigem == nodo[token].last[token].idorigem;
					});
				}

				//last_i[i] = m
				nodo[token].last[token].type = ult_prepare[token].type;
				nodo[token].last[token].m.assign(ult_prepare[token].m);
				nodo[token].last[token].idorigem = ult_prepare[token].idorigem;
				nodo[token].last[token].timestamp = ult_prepare[token].timestamp;
				nodo[token].last[token].numproposta = ult_prepare[token].numproposta;
				nodo[token].last[token].valproposta = ult_prepare[token].valproposta;

				if(maiornumerorecebido[token].first <= nodo[token].last[token].numproposta)
					maiornumerorecebido[token].first = nodo[token].last[token].numproposta;
				//deliver(m)
				std::cout <<"[DELIVER] proposta \"" << nodo[token].last[token].numproposta << "\" foi entregue para a aplicacao pelo processo " << token << "\n\n";
				//quando o processo entrega a mensagem para ele mesmo, tambem atualiza seu contador de accepts
				//mas esse processo eh apenas para prepare request
				if(nodo[token].last[token].type == 'P'){
					contresp[nodo[token].last[token].numproposta].push_back(token);
				}
			}

			//aqui: faco verificacao para enviar a mensagem apenas para quem respondeu o prepare request
			//anteriormente. Isso deve ser feito apenas quando a mensagem emitida for do tipo R (accept request)

			//enviar a todos os vizinhos corretos que pertencem ao cluster s
			if(token < N/2){
				for(int i = N - 1; i >= 0; i--){
					for(int j = 0; j < tests[i].size; j++){
						if(tests[i].vector[j] == token && nodo[i].papel[0] == 2){ //verificacao do vcube
							if(nodo[token].last[token].type == 'R'){ //se for accept request
								for(int k = 0; k < contresp[nodo[token].last[token].numproposta].size(); k++){
									if(contresp[nodo[token].last[token].numproposta][k] == nodo[i].idr && status(nodo[contresp[nodo[token].last[token].numproposta][k]].id) == 0 && nodo[contresp[nodo[token].last[token].numproposta][k]].reg == 0){
										printf("[RBCAST] processo %d envia mensagem para processo %d\n", token, nodo[i].idr);
										freceive(token, contresp[nodo[token].last[token].numproposta][k]);
										break;
									}
								}
							} else if(nodo[token].last[token].type == 'P'){
								printf("[RBCAST] processo %d envia mensagem para processo %d\n", token, nodo[i].idr);
								result = freceive(token, nodo[i].idr);
								break;
							}
						}
					}
					if(result == -1 && nodo[token].last[token].type == 'P')
					break;
				}
			} else {
				for(int i = 0; i < N; i++){
					for(int j = 0; j < tests[i].size; j++){
						if(tests[i].vector[j] == token && nodo[i].papel[0] == 2){ //verificacao do vcube
							if(nodo[token].last[token].type == 'R'){ //se for accept request
								for(int k = 0; k < contresp[nodo[token].last[token].numproposta].size(); k++){
									if(contresp[nodo[token].last[token].numproposta][k] == nodo[i].idr && status(nodo[contresp[nodo[token].last[token].numproposta][k]].id) == 0 && nodo[contresp[nodo[token].last[token].numproposta][k]].reg == 0){
										printf("[RBCAST] processo %d envia mensagem para processo %d\n", token, nodo[i].idr);
										freceive(token, contresp[nodo[token].last[token].numproposta][k]);
										break;
									}
								}
							} else if(nodo[token].last[token].type == 'P'){
								printf("[RBCAST] processo %d envia mensagem para processo %d\n", token, nodo[i].idr);
								result = freceive(token, nodo[i].idr);
								break;
							}
						}
					}
					if(result == -1 && nodo[token].last[token].type == 'P')
					break;
				}
			}

			break;



			case broadcast_acc:
			//if source(m) == i
			int result2;
			result = 0;
			if(ult_accept[token].idorigem == token && status(nodo[token].id) == 0){
				//isso aqui não vai gerar look infinito não? HAHAHAHAHAHAHAHAHAH
				while(resb){
					//existe algum elemento em ack_set que respeite <i, *, last_i[i]>?
					//retorna true, caso exista
					resb = std::any_of(nodo[token].ack_set.begin(), nodo[token].ack_set.end(), [] (auto const &elem){
						return elem.idorigem == token && !(elem.m.m.compare(nodo[token].last[token].m)) && elem.m.idorigem == nodo[token].last[token].idorigem;
					});
				}

				//last_i[i] = m
				nodo[token].last[token].type = ult_accept[token].type;
				nodo[token].last[token].m.assign(ult_accept[token].m);
				nodo[token].last[token].idorigem = ult_accept[token].idorigem;
				nodo[token].last[token].timestamp = ult_accept[token].timestamp;
				nodo[token].last[token].numproposta = ult_accept[token].numproposta;
				nodo[token].last[token].valproposta = ult_accept[token].valproposta;

				if(maiornumerorecebido[token].first <= nodo[token].last[token].numproposta)
					maiornumerorecebido[token].first = nodo[token].last[token].numproposta;
				//deliver(m)
				std::cout <<"[DELIVER] proposta \"" << nodo[token].last[token].numproposta << "\" foi entregue para a aplicacao pelo processo " << token << "\n\n";
				//quando o processo entrega a mensagem para ele mesmo, tambem atualiza seu contador de accepts
				//mas esse processo eh apenas para prepare request
				if(nodo[token].last[token].type == 'P'){
					contresp[nodo[token].last[token].numproposta].push_back(token);
				}
			}

			//aqui: faco verificacao para enviar a mensagem apenas para quem respondeu o prepare request
			//anteriormente. Isso deve ser feito apenas quando a mensagem emitida for do tipo R (accept request)

			//enviar a todos os vizinhos corretos que pertencem ao cluster s
			if(token < N/2){
				for(int i = N - 1; i >= 0; i--){
					for(int j = 0; j < tests[i].size; j++){
						if(tests[i].vector[j] == token && nodo[i].papel[0] == 2){ //verificacao do vcube
							if(nodo[token].last[token].type == 'R'){ //se for accept request
								for(int k = 0; k < contresp[nodo[token].last[token].numproposta].size(); k++){
									if(contresp[nodo[token].last[token].numproposta][k] == nodo[i].idr && status(nodo[contresp[nodo[token].last[token].numproposta][k]].id) == 0 && nodo[contresp[nodo[token].last[token].numproposta][k]].reg == 0){
										printf("[RBCAST] processo %d envia mensagem para processo %d\n", token, nodo[i].idr);
										freceive_acc(token, contresp[nodo[token].last[token].numproposta][k]);
										break;
									}
								}
							} else if(nodo[token].last[token].type == 'P'){
								printf("[RBCAST] processo %d envia mensagem para processo %d\n", token, nodo[i].idr);
								result2 = freceive_acc(token, nodo[i].idr);
								break;
							}
						}
					}
					if(result2 == -1 && nodo[token].last[token].type == 'P')
					break;
				}
			} else {
				for(int i = 0; i < N; i++){
					for(int j = 0; j < tests[i].size; j++){
						if(tests[i].vector[j] == token && nodo[i].papel[0] == 2){ //verificacao do vcube
							if(nodo[token].last[token].type == 'R'){ //se for accept request
								for(int k = 0; k < contresp[nodo[token].last[token].numproposta].size(); k++){
									if(contresp[nodo[token].last[token].numproposta][k] == nodo[i].idr && status(nodo[contresp[nodo[token].last[token].numproposta][k]].id) == 0 && nodo[contresp[nodo[token].last[token].numproposta][k]].reg == 0){
										printf("[RBCAST] processo %d envia mensagem para processo %d\n", token, nodo[i].idr);
										freceive_acc(token, contresp[nodo[token].last[token].numproposta][k]);
										break;
									}
								}
							} else if(nodo[token].last[token].type == 'P'){
								printf("[RBCAST] processo %d envia mensagem para processo %d\n", token, nodo[i].idr);
								result2 = freceive_acc(token, nodo[i].idr);
								break;
							}
						}
					}
					if(result2 == -1 && nodo[token].last[token].type == 'P')
					break;
				}
			}

			break;

	 	}
	}


	// for(int k = 0; k < quemrespondeu[token].size(); k++){
	// 	if(quemrespondeu[token][k] == nodo[i].idr && status(nodo[quemrespondeu[token][k]].id) == 0 && nodo[quemrespondeu[token][k]].reg == 0){
	// 		printf("[RBCAST] processo %d envia mensagem para processo %d\n", token, nodo[i].idr);
	// 		freceive(token, quemrespondeu[token][k]);
	// 		break;
	// 	}
	// }
	print_end(totalrodadas, N);

	return 0;
}
