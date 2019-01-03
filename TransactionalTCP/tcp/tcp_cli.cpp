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
   syn_ack,
   fin_ack,
   eof,
   ack,
   data,
   lastack
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
   int msglen;
   int packnum;
   char text[32];
};

class TTCP{
   public:  

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
						if (recvmsg->ack >=msgs[0]->seq ){
							
						
							recvmsg->options = ack;
							recvmsg->seq = recvmsg->ack;
							recvmsg->ack = recvmsg->seq + 1;
							flag = 1;

							sendto(socketid,recvmsg,sizeof(struct packet),0,(struct sockaddr *)&serverAddr,addr_size);
						}
						else
							printf("not able to establish connection with server\n");
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
					
					case fin_ack:{
						printf("received fin ack\n");
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

      msgs[0]->options = syn;
      msgs[0]->seq = seqno;
      msgs[0]->ack = seqno;
      msgs[0]->packnum = packno;
      sendto(socketid,msgs[0],sizeof(struct packet),0,(struct sockaddr *)&serverAddr,addr_size);
      printf("connection initiated: syn,fin sent\n");      

      while(!flag);  //wait till the connection is established

      while(flag){
         if(numpac < wndw && !resend){

            string str;
            getline(cin,str);
            strcpy(msgs[packno % wndw]->text, str.c_str());
            msgs[packno % wndw]->msglen = strlen(msgs[packno % wndw]->text);

            msgs[packno % wndw]->dest_port = 8082;
            msgs[packno % wndw]->src_port = 8081;
            msgs[packno % wndw]->seq = seqno;
            seqno += msgs[packno % wndw]->msglen; 

            msgs[packno % wndw]->ack = seqno;
            msgs[packno % wndw]->headerlen = 44;
            msgs[packno % wndw]->options = data;
            msgs[packno % wndw]->window_size = wndw-numpac;

            csum = 0;
            for (i=0; i< msgs[packno% wndw]->msglen; i++){
               csum += (int)msgs[packno % wndw]->text[i];
            }

            msgs[packno% wndw]->checksum = 26951 - (csum % 26951);        

            msgs[packno % wndw]->packnum = packno;
            packet_ack[packno%wndw] = 1;
            sendto(socketid,msgs[packno% wndw],sizeof(struct packet),0,(struct sockaddr *)&serverAddr,addr_size);
            printf("sent: %s-seqno:%d, options:%d\n",msgs[packno % wndw]->text,msgs[packno% wndw]->seq,msgs[packno % wndw]->options);
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
