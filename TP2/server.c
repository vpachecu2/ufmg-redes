#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include "tp_socket.h"


typedef enum { false, true } bool;

void error(char *msg) {
  perror(msg);
  exit(1);
}


int send_datagram(int so, char* buff, int buff_len, so_addr* to_addr){
    int ret;
    ret = tp_sendto(so, buff,  buff_len,  (struct sockaddr *)to_addr);
    return ret;
}

int receive_datagram(int so, char* buff, int buff_len, so_addr* from_addr){
    int ret;
    ret = tp_recvfrom(so, buff, buff_len,  (struct sockaddr *)from_addr);
    return ret;
}



int main(int argc, char **argv){
    int sock;
    int status;
    struct sockaddr_in6 sin6;
    int sin6len;   
    int portno; // id da porta
    int buffer_size; // tamanho do buffer
    int window_size; // tamanho da janela
    char *dir; // diretorio a ser utilizado
    char *filepath; // path pro arquivo a ser enviado
    char *buf; // buffer p/ mensagem
    char *hostaddrp;     
    int n; // bytesEnviados

    /* Checar argumentos da linha de comando */
    if (argc != 5) {
      fprintf(stderr, "Uso corrento: %s <porto do servidor> <tam_buffer> <tam_janela> <diretório a ser utilizado>\n", argv[0]);
      exit(1);
    }

    /* Transferindo argumentos */
    portno = atoi(argv[1]);
    buffer_size = atoi(argv[2]);
    window_size = atoi(argv[3]);
    dir = argv[4];
    filepath = argv[4];
  
    
    /* Iniciando TP */
    
    tp_init();
    sock = tp_socket(portno);    
    buf = (char*) malloc(buffer_size * sizeof(char));
       
    /* Inicializando estrutura de enderecos e protocolos*/

    memset(&sin6, 0, sin6len);
    sin6len = sizeof(struct sockaddr_in6);
    sin6.sin6_port = htons(portno);
    sin6.sin6_family = AF_INET6;
    sin6.sin6_addr = in6addr_any;
    status = getsockname(sock, (struct sockaddr *)&sin6, &sin6len);
  
    
    /* Recebendo o Nome do arquivo a ser enviado */
    char *msg;
    bool signal = false;
    int totalBytes = 0;
    int totalBytesRec = 0;

    msg = malloc(buffer_size * sizeof(char));    
    strcpy(msg, "");

    /* Recebe nome do arquivo em chuncks, pois nome do arquivo pode
    *  ser maior do que o buffer disponivel para envio no cliente
    */    
  
    while (1) {
        bzero(buf, buffer_size);
        totalBytesRec = 0;
        totalBytes = 0;
    
        memset(msg, '\0', sizeof(msg));
        memset(buf, '\0', sizeof(buf));    
        memset(filepath, '\0', sizeof(filepath));    
    
        printf("\t\tState 1\n");
        while( (n = receive_datagram(sock, buf, buffer_size, (struct sockaddr *) &sin6)) ){    
            totalBytes += n;
            totalBytesRec += strlen(buf);
            if (n < 0)
              error("ERROR in recvfrom"); // ToDo: Enviar para uma funcao de error
            
            buf[n] = '\0';
            strcat(msg, buf); //msg += buffer;
      
            if(signal && buf[n-1] == '0') 
              break;
            if (n > 1 && buf[n-2] == '\\' && buf[n-1] == '0')
              break;
            if(buf[n-1] == '\\')
              signal = true;    
            msg = realloc(msg, (strlen(msg)+n) * sizeof(char*));
            // ToDo: checar se memoria foi alocada de fato
      }
  
    msg[strlen(msg)-2] = '\0'; // remove o sinal de fim de mensagem \0 incluido no nome do arquivo
    printf("server received %d/%d bytes: %s\tsize of string: %d\n", totalBytesRec, totalBytes, msg, strlen(msg));


    /* 
     * Servidor recebeu nome do arquivo, agora vai enviar para o cliente
     */

    /* cruando Path para arquivo baseado no diretorio e filename*/
    strcat(filepath, dir);
    strcat(filepath, msg);


    FILE *file = fopen(filepath, "rb"); 

    if(file == NULL){
        printf("ERRO ABRI FILE %s", dir);
        exit(2); //Todo: passar pra funcao de erro
    }
    else
        printf("Arquivo %s aberto com sucesso\n", dir);
  
  
    
    unsigned int totalBytesSent = 0;
    unsigned int totalMsgSent = 0;
    int  numBytesSent;    
    ssize_t nElem = 0;
    int chunck_size;    
    int times = 0;    
    bool initiated = false;
    char ack[30]; // "ok: num_seq"


    printf("\t\tState 2\n");


    /* enviando arquivo em chunks */
    while(1) {        
        nElem = fread(buf, sizeof(char), buffer_size, file);       
        chunck_size =  nElem > buffer_size ? buffer_size : nElem;  
        printf("chuck size: %d, strlen buf: %d, buf size: %d, nElen: %d\n", chunck_size, strlen(buf), buffer_size, nElem);
  
        if (nElem <= 0 && !feof(file)) { 
          printf("Erro Leitura\n");
          break; // error leitura Todo: mandar pra funcao
        }
  
   /*     if(initiated == true){
           //n = recvfrom(sockfd, ack, 30, 0, (struct sockaddr *) &clientaddr, &clientlen);
           n = tp_recvfrom(sockfd, ack, 30, (struct sockaddr *) &clientaddr);
           printf("%s\n", ack);
        }*/

        numBytesSent = send_datagram(sock, buf,  chunck_size, (struct sockaddr *) &sin6);
  
        initiated = true;
        times++;
  
        if (numBytesSent < 0){
          printf("Erro Envio\n");
          break; // error sento
        }else{
          totalBytesSent += numBytesSent;
          totalMsgSent += 1;
        }
  
        if(feof(file)){
          break;
        }
      }
  
  
      
      printf("Arquivo: %s enviado para o cliente em %d blocos\n", msg, times);
  
      fclose(file);    
   
  
  
  
     shutdown(sock, 2);
     close(sock);
     return 0;
  }
  
  }  