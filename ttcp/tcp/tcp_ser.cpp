/************* server CODE *******************/


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
#include <string>
#include <algorithm>

using namespace std;


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
   
   vector<packet*> buffer;
   vector<sockaddr_in*> recvd_ips;
   int expected;

   int socketid;
   int wndw;
   int acked;
   struct packet *msgs[5];
   int seqno,packno,numpac; 
   int resend;
   FILE *fp ;
   struct sockaddr_in serverAddr, clientAddr, recvd_addr;
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

      bind(socketid, (struct sockaddr *) &serverAddr, sizeof(serverAddr));

      addr_size = sizeof serverAddr;

      for(int iter = 0; iter<wndw; iter++)
          msgs[iter] = (struct packet *)malloc(sizeof(struct packet));
   
   } //end of constructor

   void thdread(){
      int somen;
      struct packet *recvmsg = (struct packet *)malloc(sizeof(struct packet));
      while(1){
         somen = recvfrom(socketid,recvmsg,sizeof(struct packet),0,(struct sockaddr *)&recvd_addr, &addr_size) ;
         //printf("got %d %d %d\n",somen,expected,recvmsg->seq );
         if (somen > 0){
            printf("currently received:- %d %d %d\n",recvmsg->ack,recvmsg->msglen,recvmsg->options);
            buffer.push_back(recvmsg);
            recvd_ips.push_back(&recvd_addr);
         }
         else if (somen==0){
            recvmsg->ack = -1;
            sendto(socketid,recvmsg,sizeof(struct packet),0,(struct sockaddr *)&recvd_addr,addr_size);
            printf("ack sent - %d\n", recvmsg->options);
            //send ack
         }
         else{
            printf("timeout\n");
            resend = 1;
         }
      }
   }

   void process_packet(){
      struct packet *recvmsg = (struct packet *)malloc(sizeof(struct packet));
      int checksum=0, csum,CC_cache;
      struct sockaddr_in *temp_addr;
      int tao = 0;
      int onlysin = 0;

      while(1) { //while(conn)
         
         if (!buffer.empty()){
            tao = 0;
            onlysin = 0;
            string sender_ip(inet_ntoa(recvd_ips[0]->sin_addr));
            int sender_port = htons(recvd_ips[0]->sin_port);
            string findthis = sender_ip+":"+to_string(sender_port);
            recvmsg = buffer[0];
            buffer.erase(buffer.begin());
            temp_addr = recvd_ips[0];
            recvd_ips.erase(recvd_ips.begin());
                 
            switch(recvmsg->options){

               case syn:{
             	///three_whs
                recvmsg->options = syn_ack;
                recvmsg->ack = recvmsg->seq + 1;
                recvmsg->seq = 25; //random
                
                sendto(socketid,recvmsg,sizeof(struct packet),0,(struct sockaddr *)temp_addr,addr_size);
                printf("ack sent - %d\n", recvmsg->options);
				break;   
               }
               
               case fin:{
                  //send ack
                	recvmsg->options = fin_ack;
                 	sendto(socketid,recvmsg,sizeof(struct packet),0,(struct sockaddr *)&temp_addr,addr_size);
            
                	if (recvmsg->msglen > 0)
                    	printf("from client: %s\n", recvmsg->text);

					//wait for 12 sec
					//usleep(10000);
					recvmsg->options = fin;
					sendto(socketid,recvmsg,sizeof(struct packet),0,(struct sockaddr *)&temp_addr,addr_size);

					break;   
               }

               case lastack:{
                  printf("connection closed to %s\n", findthis);
                  break;
               }

               case ack:{
                  /*if(recvmsg->ack >= msgs[waiting%wndw]->ack){
                     numpac--;
                     waiting++;
                     acked = recvmsg->seq;
                     printf("received ack\n");
                  }*/
               		printf("acked recieved\n");
                	break;   
               }

               case data:{
                  printf("received: %s\n",recvmsg->text);

                  csum = 0;
                  for (int i=0; i< recvmsg->msglen; i++)
                     csum += (int)recvmsg->text[i];
                  checksum = ((csum % 26951) + recvmsg->checksum)% 26951;

                  if (checksum == 0 && recvmsg->ack == expected){
                        expected += recvmsg->msglen;
                        printf("from client: %s\n", recvmsg->text);
                        }

                  recvmsg->options = ack;   
                  recvmsg->ack = expected;        

                  printf("ACk sent: %d  %d\n", recvmsg->ack, recvmsg->seq);
                  sendto(socketid,recvmsg,sizeof(struct packet),0,(struct sockaddr *)&temp_addr,addr_size);
                  break;
               }
               default:{
                  printf("received: %d\n",recvmsg->options);   
               }
            }//end of switch
         }    
          
      }
      // string tab = "update ccount set cc="+to_string(CCgen)+" where ip=\"global\"";
      // if(mysql_query(conn, tab.c_str())!=0)
      //    cout<<mysql_error(conn)<<endl<<endl;
      return ;
   }  

};      
  
int main(){

   const char* serverip = "127.0.0.1";
   TTCP *ttcp_obj = new TTCP(serverip);

   thread tid[2];

   tid[1] = thread(&TTCP::thdread, ttcp_obj);
   tid[0] = thread(&TTCP::process_packet, ttcp_obj);


   tid[0].join();
   tid[1].join();

   return 0;
}

