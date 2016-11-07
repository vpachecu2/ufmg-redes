#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <cassert>
#include <fstream>
#include <dirent.h>
#include <bitset>
#include <pthread.h>


using namespace std;

void die(int code);

class server {
 
 private:
  struct sockaddr_in client_socket;
  int client_fd;
  socklen_t len;
 
  int port;
  int bufferSize;
  char* dir;
  char cport[128];

  template <typename T>
  std::string to_string(T value){
    std::ostringstream os ;
    os << value ;
    return os.str() ;
  }
  

  void create_server_socket (){
 
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s;
 
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET6;    /* IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
     
    s = getaddrinfo(NULL, cport, &hints, &result);
    if (s != 0) {
       //fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
       die(11);
    }
 
    /* getaddrinfo() returns a list of address structures.
      Try each address until we successfully bind(2).
      If socket(2) (or bind(2)) fails, we (close the socket
      and) try the next address. */
    socket_fd = -1;
    bool valid_connection = false;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
      socket_fd = socket(rp->ai_family, rp->ai_socktype,
               rp->ai_protocol);
      if (socket_fd == -1)
        continue;
  
      if (bind(socket_fd, rp->ai_addr, rp->ai_addrlen) == 0){
        cout << "Servidor iniciado. Porta:" << port << endl;
        valid_connection = true;
        // success
        break;
      }
 
      close(socket_fd);
    }
 
    if (not valid_connection) {
      die(3); //Error start server
    }
  }
 
 
  std::string receive_message () const{
    char buffer[bufferSize + 1];
    //memset (buffer, '\0', bufferSize);
    int nbytes;
    bool signal = false;
    std::string msg, end("\\0");

    while( (nbytes = recv (client_fd, buffer, bufferSize, 0)) ){      
      //cout << "recebido: " << buffer << "," << nbytes << endl;
      if (nbytes < 0){
        die(5); // Error accept
        //return std::string();
      }
      buffer[nbytes] = '\0';
      msg += buffer;
      if(signal and buffer[nbytes-1] == '0') 
        return msg;
      if (nbytes > 1 and buffer[nbytes-2] == '\\' and buffer[nbytes-1] == '0')
        return msg;      
      if(buffer[nbytes-1] == '\\')
        signal = true;
    }
    return std::string(); 
  }
 
 public:
  int socket_fd;
  void initialize (char* port, char* buffer, char* dir){
    this->port = atoi(port);
    this->bufferSize = atoi(buffer);
    this->dir = dir;
    strcpy (this->cport, port);
    create_server_socket();
  }
 
  ~server (){
    //close (socket_fd);
    // std::cout << "Servidor fechado\n";
  }
 
  void listen_port (){
    if (listen (socket_fd, 1) < 0){
      die(4); //Error listen
    }else{
      cout << "Listen() - Permitindo que socket ouça requisições de conexões!\nAguardando conexão...\n";
    }    
  }
 
  void accept_connection (){
    cout << " FD do Sevidor: " << socket_fd << endl;
    client_fd = accept (socket_fd, (struct sockaddr*)&(client_socket), &(len));
    if (client_fd < 0){
      die(5); // Error accept
    }else{
      cout << "Accept() - Conexão recebida e aceita!\n";
    }
    cout << "FD do Cliente: " << client_fd << endl;
  }
 
  void send_filelist(){
    DIR *pd = 0;
    struct dirent *pdirent = 0;
    std::string message;
    char buffer[bufferSize + 1];
    int sendPosition, bytesLeft;
    pd = opendir(dir);
    ssize_t numBytesSent;
    int chunck_size;

    if(pd == NULL) {
      // ERROR: wrong path or file doesnt exist
      die(8);
    }  

    cout << "Enviando lista de arquivos para o cliente: " << client_fd << endl;

    while ((pdirent=readdir(pd))){
      //int n = sprintf(tmpBuf, "$%lu$%s", strlen(pdirent->d_name), pdirent->d_name);
      message = to_string(pdirent->d_name) + "\\n";
      bytesLeft = message.size();
      sendPosition = 0;
      while(bytesLeft > 0){
        chunck_size = bytesLeft > bufferSize ? bufferSize : bytesLeft;
        memcpy(buffer, message.c_str() + sendPosition, chunck_size);
        numBytesSent = send(client_fd, message.c_str() + sendPosition, chunck_size, 0);        
        if (numBytesSent < 0){
          die(7); //error send
        }/*else if(bytesLeft < 0){
          cout << "send() - Enviando mensagem para o cliente (" << pdirent->d_name << ")!\n";
        }*/
        sendPosition += numBytesSent;
        bytesLeft -= numBytesSent;        
      }      
    }    
    numBytesSent = send(client_fd, "\\0", 2, 0);
  }

  void send_file(string name){
    char buffer[bufferSize];
    std::string filename = "";
    for (unsigned i = 0; i < name.size(); i++){
      if(name.c_str()[i] == '\\' && name.c_str()[i+1] == '0') break;
      filename += name.c_str()[i];
    }
    filename = to_string(dir) + to_string("/") + filename;
    FILE *file = fopen(filename.c_str(), "rb"); // open in binary mode
    
    if(file == NULL)
      die(8); //file doesnt exist

    //unsigned int bytesRead = 0;
    unsigned int totalBytesSent = 0;
    unsigned int totalMsgSent = 0;
    
    ssize_t nElem = 0;
    cout << "Enviando arquivo: " << filename << " para o cliente: " << client_fd << endl;
    while(true) {
      nElem = fread(&buffer, sizeof(char), bufferSize, file); 
      if (nElem <= 0 && !feof(file)) { 
        die(9); // reading error;
      }

      ssize_t numBytesSent = send(client_fd, buffer, nElem, 0);
      if (numBytesSent < 0){
        die(7); //error send
      }else{
        totalBytesSent += numBytesSent;
        totalMsgSent += 1;
        //cout << "send() - Bytes ja enviados: " << totalBytesSent << endl;
      }

      if(feof(file)){
        /*cout << "End of file\n";
        cout << totalMsgSent << endl;*/
        break;
      }
    }
    cout << "Arquivo: " << filename << " enviado para o cliente: " << client_fd << endl;
    fclose(file);
  }

  bool receive_from_client (){
    std::string str;
    str = receive_message();
    if(str == "list\\0"){
      send_filelist();
      return true;
    //if string starts with get
    }else if(str.compare(0, 3, "get") == 0){
   		//gets filename
	   	string filename = str.substr(4, str.length() - 4);
      send_file(filename);
      return true;
    }
    return false;
  }
 
  void close_connection_with_client (){
    cout << "Conexao fechada, id socket: " << socket_fd << endl;
    close(client_fd);
  }
   
};
 
typedef struct {
  server s;
  bool status;
  int id;
} thread_arg, * ptr_thread_arg;

void* create_thread(void *arg) {
  ptr_thread_arg argument = (ptr_thread_arg)arg;
  cout << "Thread ID: " <<argument->id << "\n";
  bool status = argument->s.receive_from_client();
  argument->status = status;
  if(status)
    argument->s.close_connection_with_client();
}

void catch_errors(int argc, char** argv){
  if(argc < 4){
    die(1);
  }
}
 
int main (int argc, char* argv[]){
  int MAXTHREADS = 100;
  pthread_t threads[MAXTHREADS];    // ponteiro para as threads criadas
  thread_arg arguments[MAXTHREADS]; 
  int current_thread = 0;
  server s;

  catch_errors(argc, argv);

  s.initialize(argv[1], argv[2], argv[3]);  
  // server s;
  s.listen_port();
  while (true){
    if(current_thread >= MAXTHREADS) current_thread = 0;
    s.accept_connection();    
    arguments[current_thread].s = s;
    arguments[current_thread].id = current_thread;
    pthread_create(&(threads[arguments[current_thread].id]), NULL, create_thread, &(arguments[current_thread]));
    current_thread++;
  }
  return 0;
}

void die(int code){
  switch(code){
    case 1:
      cerr << "Erro: -1 - Descrição: Erros nos argumentos de entrada\n";
      exit(-1);        
    case 2:
      cerr << "Erro: -2 - Descrição: Erro de criação de socket\n" ;
      exit(-2);
    case 3:
      cerr << "Erro: -3 - Descrição: Erro de bind - Porta ja utilizada\n" ;
      exit(-3);
    case 4:
      cerr << "Erro: -4 - Descrição: Erro de listen\n" ;
      exit(-4);
    case 5:
      cerr << "Erro: -5 - Descrição: Erro de accept\n" ;
      exit(-5);
    case 6:
      cerr << "Erro: -6 - Descrição: Erro de connect\n" ;
      exit(-6);
    case 7:
      cerr << "Erro: -7 - Descrição: Erro de comunicação com servidor/cliente\n" ;
      exit(-7);
    case 8:
      cerr << "Erro: -8 - Descrição: Arquivo solicitado não encontrado\n" ;
      exit(-8);
    case 9:
      cerr << "Erro: -9 - Descrição: Erro em ponteiro\n" ;
      exit(-9);
    case 10:
      cerr << "Erro: -10 - Descrição: Comando de clienteFTP não existente\n" ;
      exit(-10);
    case 11:
      cerr << "Erro: -11 - Descrição: Problema ai extrair informacoes de getaddrinfo\n" ;
      exit(-11);
    default:
      cerr << "Erro: -999 - Descrição: Error nao listado\n" ;
      exit(-999);
  }
}
