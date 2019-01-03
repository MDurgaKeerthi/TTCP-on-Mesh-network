/************* CLIENT CODE *******************/


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
#include <unistd.h>
#include <malloc.h>
#include <vector>
#include <iostream>
// #include <mysql/mysql.h>
// #include <cstdio>
#include <string>


using namespace std;

/*struct control_block{
   int CCsend;
   int CCrecv;
   int conn_active;
};*/

enum option_names{
   syn,
   fin,
   syn_fin,
   syn_ack,
   fin_ack,
   syn_fin_ack,
   eof,
   ack,
   data,
   cc_echo,
   cc_new,
   three_whs,
   cc,
   lastack
};
//client will not recieve syn,syn_fin,data,cc_new

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
   int cc_option;
   int msglen;
   int packnum;
   char text[32];
};

class TTCP{
   public:  
   int CCgen, CCsend, CCrecv;
   // MYSQL *conn;
   // MYSQL_RES *res;
   // MYSQL_ROW row;

   int socketid;
   int wndw;
   int acked;
   int packet_ack[5];
   struct packet *msgs[5];
   int seqno,packno,numpac; 
   int resend;
   FILE *fp ;
   struct sockaddr_in serverAddr, clientAddr;
   struct sockaddr_storage serverStorage;
   socklen_t addr_size;
   int waiting;
   int flag;

   TTCP(const char* serverip){
      // char *server = "127.0.0.1";
      // char *user = "root";
      // char *password = "begood";
      // char *database = "ttcp_sample";
      // conn = mysql_init(NULL);
      // mysql_real_connect(conn, server, user, password, database, 0, NULL, 0);


      // //read your global count on start
      // string tab = "select cc from ccount1 where ip=\"global\"";
      // mysql_query(conn, tab.c_str());
      // res = mysql_use_result(conn);
      // row = mysql_fetch_row(res);
      // CCgen = strtol(row[0],NULL,0);
      // printf("%d\n", CCgen);
      CCgen = 0;
      CCrecv = 0;
      CCsend = CCgen;

      socketid = 0;
      wndw = 5;
      acked = -1;
      seqno = 0;
      packno=0;
      numpac=0; 
      resend = 0;
      waiting=0;
      flag=1;

      socketid = socket(PF_INET, SOCK_DGRAM, 0);

      //server on 8081, client on 8082
      serverAddr.sin_family = AF_INET;
      serverAddr.sin_port = htons(8081);
      serverAddr.sin_addr.s_addr = inet_addr(serverip);
      memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero); 

      clientAddr.sin_family = AF_INET;
      clientAddr.sin_port = htons(8082);
      clientAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
      memset(clientAddr.sin_zero, '\0', sizeof clientAddr.sin_zero);  

      bind(socketid, (struct sockaddr *) &clientAddr, sizeof(clientAddr));

      addr_size = sizeof serverAddr;

      for(int iter = 0; iter<wndw; iter++){
         packet_ack[iter] = 0;
         msgs[iter] = (struct packet *)malloc(sizeof(struct packet));
      }

      // struct timeval tv;
      // tv.tv_sec= 12;
      // tv.tv_usec = 0;
      // int cv = setsockopt(socketid, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval));
      // printf("%d", cv); 
   
   } //end of constructor

   void thdread(){
      struct packet *recvmsg = (struct packet *)malloc(sizeof(struct packet));
      int somen, expected=0, checksum=0;

      while(1) { //while(conn)
         somen = recvfrom(socketid,recvmsg,sizeof(struct packet),0,(struct sockaddr *)&serverAddr, &addr_size) ; 
         printf("currently received:- %d - %d %d --- %d\n",recvmsg->ack,recvmsg->seq,msgs[waiting%wndw]->ack, recvmsg->options);
         
         if (somen>0 ){   
            switch(recvmsg->options){  //fin,syn_ack,fyn_ack,syn_fyn_ack,eof,ack

               case syn_ack:{
                  
                  if(recvmsg->cc_option == cc_echo){

                     flag = 1;
                     CCrecv = recvmsg->connection_count;
                     CCgen = CCrecv;
                  }
                  else if(recvmsg->cc_option == three_whs){
                     //send ack
                     if (recvmsg->ack >=msgs[0]->seq ){
                        recvmsg->options = ack;
                        recvmsg->seq = recvmsg->ack;
                        recvmsg->ack = recvmsg->seq + 1;
                        flag = 1;
                        CCrecv = recvmsg->connection_count;
                        sendto(socketid,recvmsg,sizeof(struct packet),0,(struct sockaddr *)&serverAddr,addr_size);
                     }
                     else
                        printf("not able to establish connection with server\n");
                  }
                  break;
               }
               
               case fin:{
                  //send ack
                  recvmsg->options = lastack;
                  sendto(socketid,recvmsg,sizeof(struct packet),0,(struct sockaddr *)&serverAddr,addr_size);
                  //wait for 12 sec
                  //usleep(10000);
                  break;   
               }
               case syn_fin_ack:
               case fin_ack:{
                  CCrecv = recvmsg->connection_count;  
                  CCgen = CCrecv;
               }
               case ack:{
                  if (recvmsg->ack >= seqno)
                     packet_ack[recvmsg->packnum % wndw] = 0;
                  if(recvmsg->ack >= msgs[recvmsg->packnum % wndw]->ack){
                     numpac--;
                     waiting++;
                     acked = recvmsg->seq;
                     printf("received ack\n");
                  }
                  break;   
               }
               default:{
                  printf("received: %d\n",recvmsg->options);   
               }
            }
         }    
         else{
            printf("timeout\n");
            resend = 1;
         } 
      }
      return ;
   }  

   void thdsend(){
      int iter = 0;
      int i, csum=0; 
      int runit = 100;
      msgs[0]->options = syn_fin;
      msgs[0]->seq = seqno;
      msgs[0]->ack = seqno;
      msgs[0]->packnum = packno;
      msgs[0]->connection_count = CCgen;
      CCsend = CCgen;
      msgs[0]->cc_option = cc_new;
      //CCgen++;
      sendto(socketid,msgs[0],sizeof(struct packet),0,(struct sockaddr *)&serverAddr,addr_size);
      printf("connection initiated: syn,fin sent\n");      

      while(!flag);  //wait till the connection is established

      while(flag){
         if (runit >=100)
         {
            return;
            /* code */
         }
         if(numpac < wndw && !resend){

            string str = "this is message #"+to_string(runit);
            runit++;
            strcpy(msgs[packno % wndw]->text, str.c_str());
            msgs[packno % wndw]->msglen = strlen(msgs[packno % wndw]->text);

            msgs[packno % wndw]->dest_port = 8082;
            msgs[packno % wndw]->src_port = 8081;
            msgs[packno % wndw]->seq = seqno;
            seqno += msgs[packno % wndw]->msglen; 

            msgs[packno % wndw]->ack = seqno;
            msgs[packno % wndw]->headerlen = 44;
            msgs[packno % wndw]->options = syn_fin;
            msgs[packno % wndw]->window_size = wndw-numpac;

            csum = 0;
            for (i=0; i< msgs[packno% wndw]->msglen; i++){
               csum += (int)msgs[packno % wndw]->text[i];
            }

            msgs[packno% wndw]->checksum = 26951 - (csum % 26951);        
            CCgen++;
            msgs[packno % wndw]->connection_count = CCgen;   
            CCsend = CCgen;
            
            msgs[packno%wndw]->cc_option = cc;

            msgs[packno % wndw]->packnum = packno;
            packet_ack[packno%wndw] = 1;
            sendto(socketid,msgs[packno% wndw],sizeof(struct packet),0,(struct sockaddr *)&serverAddr,addr_size);
            printf("sent: %s-seqno:%d, options:%d %d\n",msgs[packno % wndw]->text,msgs[packno% wndw]->seq,msgs[packno % wndw]->options, msgs[packno % wndw]->cc_option);
            packno++;
            numpac++;
         }  

         if(resend){
            for(iter=0; iter<wndw; iter++){
               if(packet_ack[iter]){
                  sendto(socketid,msgs[iter],sizeof(struct packet),0,(struct sockaddr *)&serverAddr,addr_size);
                  printf("packet resent with seq:%d\n",msgs[iter]->seq);
               }   
            }
            resend = 0;
         }        
      }
       
      // string tab = "update ccount1 set cc="+to_string(CCgen)+" where ip=\"global\"";
      // if(mysql_query(conn, tab.c_str())!=0)
      //    cout<<mysql_error(conn)<<endl<<endl;
                  
      return;
   }
};      
  

int main(){

   const char* serverip = "127.0.0.1";
   TTCP *ttcp_obj = new TTCP(serverip);
  
   thread tid[2];

   tid[1] = thread(&TTCP::thdread, ttcp_obj);
   tid[0] = thread(&TTCP::thdsend, ttcp_obj);

   tid[0].join();
   tid[1].join();

   return 0;
}
