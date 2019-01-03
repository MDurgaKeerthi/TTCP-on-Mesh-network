/************* UDP CLIENT CODE *******************/
//sends file
//SENDER

#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <thread>
#include <time.h>
#include <malloc.h>
#include <vector>
#include <iostream>
// #include <mysql/mysql.h>
#include <cstdio>


using namespace std;

struct control_block{
  int CCsend;
  int CCrecv;
  int conn_active;
};

enum option_names{
  syn,
  fyn,
  eof,
  ack,
  data
  };

struct packet{
  int src_port;
  int dest_port;
  int seq;
  int ack;
  int headerlen;
  int options;
  int window_size;
  int checksum;
  int connection_count;
  int msglen;
  int packnum;
  char text[32];
  };

class TTCP{
  public:  
    int CC;
    // vector <control_block> TCB;
    // MYSQL *conn;
    // MYSQL_RES *res;
    // MYSQL_ROW row;

    int socketid;
    int wndw;
    int acked;
    struct packet *msgs[5];
    int seqno,packno,rem, numpac; 
    int resend;
    FILE *fp ;
    struct sockaddr_in serverAddr, clientAddr;
    struct sockaddr_storage serverStorage;
    socklen_t addr_size;
    int waiting;

    TTCP(){

      char *server = "127.0.0.1";
      char *user = "root";
      char *password = "begood";
      char *database = "ttcp_sample";
      conn = mysql_init(NULL);
      mysql_real_connect(conn, server, user, password, database, 0, NULL, 0);

      CC = 0;
      socketid = 0;
      wndw = 5;
      acked = -1;
      seqno = 0;
      packno=0;
      numpac=0; 
      resend = 0;
      waiting=0;

      socketid = socket(PF_INET, SOCK_DGRAM, 0);

      //server on 8081, client on 8082
      serverAddr.sin_family = AF_INET;
      serverAddr.sin_port = htons(8081);
      serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
      memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero); 

      clientAddr.sin_family = AF_INET;
      clientAddr.sin_port = htons(8082);
      clientAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
      memset(clientAddr.sin_zero, '\0', sizeof clientAddr.sin_zero);  

      bind(socketid, (struct sockaddr *) &clientAddr, sizeof(serverAddr));

      addr_size = sizeof serverAddr;

      fp = fopen("sample.txt", "r");
      fseek(fp, 0L, SEEK_END);
      int sz = ftell(fp);
      fseek(fp, 0L, SEEK_SET);
      rem = sz;
      }

    void thdread(){
      struct packet *recvmsg = (struct packet *)malloc(sizeof(struct packet));
      int somen;
      int flag = 1;
      int expected=0, checksum=0;
      while(flag) { //while(conn)
        somen = recvfrom(socketid,recvmsg,sizeof(struct packet),0,(struct sockaddr *)&serverAddr, &addr_size) ; 
        printf("currently received:- %d - %d %d --- %d\n",recvmsg->ack,recvmsg->seq,msgs[waiting%wndw]->ack, recvmsg->options);
        //printf("received: %s\n",recvmsg->text);
        if (recvmsg->options == ack){         
          if(somen>=0 && recvmsg->ack >= msgs[waiting%wndw]->ack){
            numpac--;
            waiting++;
            acked = recvmsg->seq;
            //printf("currently acked:- %d - %d\n",recvmsg->ack,msgs[0]->ack);
            waiting += 1020;
            printf("received ack\n");
            if (recvmsg->options == eof){
              flag = 0;
              cout<<"ended"<<endl;
              }
            }
          else if(somen < 0){
            printf("timeout\n");
            resend = 1;
            } 
          }
        else if(recvmsg->options == data){
          printf("received: %s\n",recvmsg->text);

          if (expected == recvmsg->seq && checksum ==0 ){
             if(somen > 0){
                expected += recvmsg->msglen;
                recvmsg->ack =  expected;
                recvmsg->options = ack;
                //rec<<msg->text;
                }
             else
                recvmsg->ack = expected;
                }
          else
             recvmsg->ack = expected;        

          printf("ACk sent: %d  %d\n", recvmsg->ack, recvmsg->seq);
          sendto(socketid,recvmsg,sizeof(struct packet),0,(struct sockaddr *)&serverAddr,addr_size);
           
          }     

        }
      return ;
      }  

    void thdsend(){
       int iter = 0;
       for(iter = 0; iter<wndw; iter++)
          msgs[iter] = (struct packet *)malloc(sizeof(struct packet));

       int i, csum=0; 
       while(1){
        //while(numpac < wndw && !resend){
        //cout<<"enter:";        
           msgs[packno % wndw]->seq = seqno;
           
           //fread(msgs[packno % wndw]->text,1,1020,fp);
           string str;
           getline(cin,str);
           strcpy(msgs[packno % wndw]->text, str.c_str());
           msgs[packno % wndw]->msglen = strlen(msgs[packno % wndw]->text);

           seqno += msgs[packno % wndw]->msglen; 
           rem -= msgs[packno % wndw]->msglen;  
           msgs[packno % wndw]->ack = seqno;
           
           csum = 0;
           for (i=0; i< msgs[packno% wndw]->msglen; i++){
             csum += (int)msgs[packno % wndw]->text[i];
           }
           
           msgs[packno % wndw]->packnum = packno; 
           msgs[packno% wndw]->checksum = 26951 - (csum % 26951);
           msgs[packno % wndw]->options = data;

           /*cout<<msgs[packno % wndw]->text<<endl; 
           cout<<wndw<<endl; 
           cout<<msgs[packno% wndw]->checksum<<endl;*/
           
           sendto(socketid,msgs[packno% wndw],sizeof(struct packet),0,(struct sockaddr *)&serverAddr,addr_size);
           //printf("packet sent with seq:%d\n",msgs[packno% wndw]->seq);
           printf("sent: %s-seqno:%d, options:%d\n",msgs[packno % wndw]->text,msgs[packno% wndw]->seq,msgs[packno % wndw]->options);
           packno++;
           numpac++;
          //}  

        while(resend){
          for(iter=0; iter<wndw; iter++){
             sendto(socketid,msgs[iter],sizeof(struct packet),0,(struct sockaddr *)&serverAddr,addr_size);
             printf("packet resent with seq:%d\n",msgs[iter]->seq);
             }
          resend = 0;
          }        
        }
          
       return;
       }
  };      
  
int main(){

  TTCP *ttcp_obj = new TTCP();
  
  thread tid[2];
  
  tid[1] = thread(&TTCP::thdread, ttcp_obj);
  tid[0] = thread(&TTCP::thdsend, ttcp_obj);
  

  tid[0].join();
  tid[1].join();
  
  return 0;
}
