#include <iostream>
#include <vector>
// #include "LinkState.h"
// #include "client.h"
// #include "server.h"
#include "DistanceVector.h"
#define COSTMATRIXSIZE 2

void init(string filename);

struct packet {

	int src;
	int dst;
	int nexthop;
	subPacket message;
	
};

void receivemsg() {


	char recvData[1024];
	int nBytes;

	struct sockaddr_in remoteaddr;
	socklen_t addr_size = sizeof(remoteaddr);

	
	while(true) {

    	nBytes = recvfrom(sockConn,recvData,1024,0,(struct sockaddr *)&remoteaddr, &addr_size);
		struct packet *buffer=(struct packet*)malloc(sizeof(struct packet));

    	buffer=(struct packet*)recvData;


    	if(nBytes!=-1) {

    		int a, b;
    		a=buffer->nexthop;

    		if(buffer->dst==routerID) {
    			cout<<"Received Message from : "<<buffer->src<<endl; 
    		} else {

    			int fwdto=buffer->dst;
    			buffer->nexthop=path[fwdto][0];
				string packetIP=(router[path[fwdto][0]].first);
				int packetPort=(router[path[fwdto][0]].second);
    			b=buffer->nexthop;
				cout<<"Forwarding packet from router "<<inet_ntoa(remoteaddr.sin_addr)<<":"
					<<ntohs(remoteaddr.sin_port)<<" to "<<packetIP<<":"<<packetPort<<endl;
				struct sockaddr_in remoteaddr;
				remoteaddr.sin_family = AF_INET;
				
				remoteaddr.sin_addr.s_addr = inet_addr(packetIP.c_str());
				remoteaddr.sin_port = htons(packetPort);
				socklen_t addr_size = sizeof(remoteaddr);
				struct subPacket *inp=(struct subPacket*)malloc(sizeof(struct subPacket));
				inp->myNodeID=routerID;
				inp->myNeigh=costMatrix;

				sendto(sockConn,buffer,sizeof(*buffer),0,(struct sockaddr *)&remoteaddr,addr_size);
    		}
    	}
	}


}

int main(int argc, char const *argv[])
{
	if(argc<4) {
		cout<<"Usage: ./a.out PortNumber1 PortNumber2 inputFile\n";
		return -1;
	}

	int myPort=atoi(argv[1]);
	string filename=argv[3];

	init(filename);

	// LinkState(myPort);
	DistanceVector(myPort);
	
	
	struct sockaddr_in server,client;
	int sockaddr_len=sizeof(struct sockaddr_in);

	if((sockConn=socket(AF_INET, SOCK_DGRAM, 0))==-1) {
		perror("socket: ");
		exit(-1);
	}

	server.sin_family = AF_INET;
	server.sin_port=htons(atoi(argv[2]));
	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	bzero(&server.sin_zero,8);


	if(bind(sockConn, (struct sockaddr *)&server, sockaddr_len)==-1) {
		perror("Bind");
		exit(-1);
	}

	thread myThread1 (receivemsg);


	int dstID;
	cout<<"Enter destination routerID: ";
	cin>>dstID;

	cout<<"My IP and Port are "<<inet_ntoa(server.sin_addr)<<":"<<ntohs(server.sin_port)<<endl;

	if(dstID!=-1) {

		struct packet *buffer=(struct packet*)malloc(sizeof(struct packet));
		buffer->src=routerID;
		buffer->dst=dstID;

		string packetIP;
		int packetPort;

		if(path[dstID].size()==1) {
			buffer->nexthop=-1;
		}
		else {
			buffer->nexthop=path[dstID][1];
		}

		packetIP=(router[path[dstID][0]].first);
		packetPort=(router[path[dstID][0]].second);

		cout<<"Send to "<<packetIP<<" "<<packetPort<<endl;
		struct sockaddr_in remoteaddr;
		remoteaddr.sin_family = AF_INET;
		
		remoteaddr.sin_addr.s_addr = inet_addr(packetIP.c_str());
		remoteaddr.sin_port = htons(packetPort);
		socklen_t addr_size = sizeof(remoteaddr);
		struct subPacket *inp=(struct subPacket*)malloc(sizeof(struct subPacket));
		inp->myNodeID=routerID;
		inp->myNeigh=costMatrix;
		sendto(sockConn,buffer,sizeof(*buffer),0,(struct sockaddr *)&remoteaddr,addr_size);

	}

	myThread1.join();
	return 0;
}

void init(string filename) {

	ifstream input;
  	input.open(filename);
	
	int neighbours;
	input>>routerID;
	input>>neighbours;

	for(int i=0;i<COSTMATRIXSIZE;i++) {
		if(i==routerID) {
			costMatrix[i]=0;
		} else {
			costMatrix[i]=1000;
		}
	}

	int weight, port1,port2, neigh;
	string ip;

	for (int i = 0; i < COSTMATRIXSIZE; ++i) {
		parentDV[i]=INT_MAX;
	}
	parentDV[routerID]=-1;


	for(int i=0;i<neighbours;i++) {
		input>>neigh>>ip>>port1>>port2>>weight;
		
		costMatrix[neigh]=weight;
		pair<string, int> p;
		p.first=ip;
		p.second=port1;
		neighID[p]=neigh;
		Ports[neigh]=port1;
		parentDV[neigh]=routerID;
		pair<string, int> p2;
		p2.first=ip;
		p2.second=port2;
		router[neigh]=p2;

	}

	cout<<"::::::::::::::::::::::::::::::::::::\n";
	
	cout<<"I am connected to: \n";
	for (std::map<pair<string, int>,int>::iterator it=neighID.begin(); it!=neighID.end(); ++it) {

		pair<string, int> p = it->first;
		cout<<it->second<<"-"<<p.first<<":"<<p.second<<endl;
	}
	cout<<"::::::::::::::::::::::::::::::::::::\n";

}