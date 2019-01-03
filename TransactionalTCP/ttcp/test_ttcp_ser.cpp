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

#include <mysql/mysql.h>
#include <cstdio>

using namespace std;

struct control_block{
   int CCsend;
   int CCrecv;
   int conn_active;
};

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

//server will not recieve syn_ack, fin_ack,syn_fin_ack,cc_echo

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
   vector <string> ips_ports;
   std::vector<int> prev_cc;
   std::vector<packet*> buffer;
   std::vector<sockaddr_in*> recvd_ips;
   std::vector<int> expected;

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

   MYSQL *conn;
   MYSQL_RES *res;
   MYSQL_ROW row;

   TTCP(const char* serverip){
      char *server = "127.0.0.1";
      char *user = "root";
      char *password = "begood";
      char *database = "ttcp_sample";
      conn = mysql_init(NULL);
      mysql_real_connect(conn, server, user, password, database, 0, NULL, 0);

      CCgen = 0;

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
            printf("currently received:- %d %d %d %d\n",recvmsg->ack,recvmsg->msglen,recvmsg->cc_option,recvmsg->options);
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
               case syn:
                  onlysin =1;
               case syn_fin:{

                  CCrecv = recvmsg->connection_count;
                  
                  CCgen++;
                  CCsend = CCgen;
                  
                  //check from database
                  //if ok
                     //send CCgen
                     //allocate buffer
                     //process data
                  //else
                     //do three_whs

                  if (recvmsg->cc_option == cc){
                     
                     //cout<<findthis;
                     auto it = find (ips_ports.begin(), ips_ports.end(), findthis);
                     auto pos = distance(ips_ports.begin(), it);
                     CC_cache = prev_cc[pos];
                     printf("%d  %d\n",CC_cache,recvmsg->connection_count );
                     if (CC_cache <= recvmsg->connection_count ){//tao
                        tao = 1;
                        CC_cache = CCrecv;
                        
                        if (recvmsg->msglen > 0){
                           csum = 0;
                           for (int i=0; i< recvmsg->msglen; i++){
                              csum += (int)recvmsg->text[i];
                           }

                           checksum = ((csum % 26951) + recvmsg->checksum)% 26951;
                           printf("%d - checksum\n", checksum);
                           
                           if (checksum == 0 && recvmsg->seq == expected[pos]){
                              expected[pos] += recvmsg->msglen;  
                              printf("received: %s-%d\n",recvmsg->text, recvmsg->msglen );

                              //write data to server
                              string msg_text(recvmsg->text);
                              string tab = "insert into server_recvd_data(ip, port, data) values(\""+sender_ip+"\","+to_string(sender_port)+",\""+msg_text+"\")";
                              if(mysql_query(conn, tab.c_str())!=0)
                                 cout<<mysql_error(conn)<<endl;
                           } 
                        }  

                        recvmsg->ack =  expected[pos];
                        recvmsg->options = syn_fin_ack;
                        recvmsg->cc_option = cc_echo;
                        sprintf(recvmsg->text,"%d", CCrecv);  
                        recvmsg->connection_count = CCsend;    
                        
                        prev_cc[pos] = CCsend; 
                              
                     }
                     else{ //three_whs
                        recvmsg->options = syn_ack;
                        recvmsg->cc_option = three_whs;
                        recvmsg->ack = recvmsg->seq + 1;
                        recvmsg->seq = CCgen;
                        recvmsg->connection_count = CCgen;
                        auto it = find (ips_ports.begin(), ips_ports.end(), findthis);
                        auto pos = distance(ips_ports.begin(), it);
                        if (pos >= ips_ports.size()){
                           ips_ports.push_back(sender_ip+":"+to_string(sender_port));
                           prev_cc.push_back(CCgen);
                           expected.push_back(0);
                        }
                        else{
                           prev_cc[pos] = CCgen;
                           expected[pos] = 0;
                        }
                     }
                     
                  }   
                  else { //three_whs - fallback to tcp
                     if (recvmsg->cc_option == cc_new)
                        CC_cache = -1;
                     printf("connection_count sent  %d\n",CCgen );
                     recvmsg->options = syn_ack;
                     recvmsg->cc_option = three_whs;
                     recvmsg->ack = recvmsg->seq + 1;
                     recvmsg->seq = CCgen;
                     recvmsg->connection_count = CCgen;
                     auto it = find (ips_ports.begin(), ips_ports.end(), findthis);
                     auto pos = distance(ips_ports.begin(), it);
                     if (pos >= ips_ports.size()){
                        ips_ports.push_back(sender_ip+":"+to_string(sender_port));
                        prev_cc.push_back(CCgen);
                        expected.push_back(0);
                     }
                     else{
                        prev_cc[pos] = CCgen;
                        expected[pos] = 0;
                     }
                  }
                  
                  if (onlysin)
                     recvmsg->options = syn_ack;
                  
                  sendto(socketid,recvmsg,sizeof(struct packet),0,(struct sockaddr *)temp_addr,addr_size);
                  printf("ack sent - %d\n", recvmsg->options);

                  if(recvmsg->options == syn_fin_ack){
                     recvmsg->options = fin;
                     sendto(socketid,recvmsg,sizeof(struct packet),0,(struct sockaddr *)&temp_addr,addr_size);
                  }
                  break;   
               }
               
               case fin:{
                  //send ack
                  recvmsg->options = fin_ack;
                  sendto(socketid,recvmsg,sizeof(struct packet),0,(struct sockaddr *)&temp_addr,addr_size);
            
                  if (recvmsg->msglen > 0)
                     printf("from client: %s\n", recvmsg->text);
                  //write data to server
                  string msg_text(recvmsg->text);
                  string tab = "insert into server_recvd_data(ip, port, data) values(\""+sender_ip+"\","+to_string(sender_port)+",\""+msg_text+"\")";
                  if(mysql_query(conn, tab.c_str())!=0)
                     cout<<mysql_error(conn)<<endl;

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
                  break;   
               }
               case data:{
                  printf("received: %s\n",recvmsg->text);

                  csum = 0;
                  for (int i=0; i< recvmsg->msglen; i++)
                     csum += (int)recvmsg->text[i];
                  checksum = ((csum % 26951) + recvmsg->checksum)% 26951;

                  if (checksum == 0 ){
                     
                        //expected += recvmsg->msglen;
                        printf("from client: %s\n", recvmsg->text);

                        //write data to server
                        string msg_text(recvmsg->text);
                        string tab = "insert into server_recvd_data(ip, port, data) values(\""+sender_ip+"\","+to_string(sender_port)+",\""+msg_text+"\")";
                        if(mysql_query(conn, tab.c_str())!=0)
                           cout<<mysql_error(conn)<<endl;
                        }

                  recvmsg->options = ack;   
                  //recvmsg->ack = expected;        

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

