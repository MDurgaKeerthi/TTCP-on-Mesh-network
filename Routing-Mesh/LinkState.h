#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <array>
#include <vector>
#include <thread>        
#include <map>
#include <utility>
#include <climits>
#include <mutex>
#include <queue>
#include <fstream>
#include <atomic>
#include<set>
#define COSTMATRIXSIZE 2
#define MAX_subPacket 1024

using namespace std;

std::map<pair<string, int>, int> neighID;
std::map<int, pair<string, int>> router;
std::array<int, COSTMATRIXSIZE> costMatrix;
std::map<int, int> Ports;
std::vector<int> listOfPort;
int sockConn, routerID, sock;
std::vector<std::vector<int> > path(COSTMATRIXSIZE);
std::queue<struct subPacket> myqueue;
mutex mtx;
bool timerOver;

int parentDV[COSTMATRIXSIZE];

// Packet
struct subPacket{
	int myNodeID;
	std::array<int, COSTMATRIXSIZE> myNeigh;
};


// Send Function
void sendFunc(std::map<int, int> Ports, std::array<int, COSTMATRIXSIZE> costMatrix) {

	for(int i=0;i<listOfPort.size();i++) {
		struct sockaddr_in remoteaddr;
		remoteaddr.sin_family = AF_INET;
		string s = "192.168.0.255";
		remoteaddr.sin_addr.s_addr = inet_addr(s.c_str());
		remoteaddr.sin_port = htons(listOfPort[i]);
		socklen_t addr_size = sizeof(remoteaddr);
		struct subPacket *inp=(struct subPacket*)malloc(sizeof(struct subPacket));
		inp->myNodeID=routerID;
		inp->myNeigh=costMatrix;
    	sendto(sock,inp,sizeof(*inp),0,(struct sockaddr *)&remoteaddr,addr_size);
	}
}

// Receive Function
void receiveFunc() {

	char buffer[1024];
	int nBytes;
	struct sockaddr_in remoteaddr;
	socklen_t addr_size = sizeof(remoteaddr);

	set<int> st;
	while( st.size() != COSTMATRIXSIZE-1) {
    	nBytes = recvfrom(sock,buffer,1024,0,(struct sockaddr *)&remoteaddr, &addr_size);
		if(nBytes!=-1) {
			struct subPacket *inp=(struct subPacket*)malloc(sizeof(struct subPacket));
	    	inp=(struct subPacket*)buffer;
	    	mtx.lock();
	    	myqueue.push(*inp);
	    	mtx.unlock();
	    	st.insert(inp->myNodeID);
		}
        
	}
}

// Function to print
void printPath(int parent[], int j, int idx) {
    if (parent[j] == - 1)
        return;
    printPath(parent, parent[j],idx);
    printf("%d ", j);
    path[idx].push_back(j);
}

// Function to print
int printSolution(int dist[], int n, int parent[]) {
    int src = routerID;
    printf("Vertex\t Distance\tPath");
    for (int i = 0; i < COSTMATRIXSIZE; i++) {
    	if(i==routerID)
    		continue;
        printf("\n%d -> %d \t\t %d\t\t%d ",src, i, dist[i], src);
        printPath(parent, i, i);
    }
}


int minDistance(int dist[], bool sptSet[]) {
	int min = INT_MAX, min_index;

	for (int v = 0; v < COSTMATRIXSIZE; v++) {
		if (sptSet[v] == false && dist[v] <= min) {
			min = dist[v], min_index = v;
		}
	}

	return min_index;
}

// Function to calculate distance
void distance (std::array<int, COSTMATRIXSIZE> costMatrix) {

	std::array<std::array<int, COSTMATRIXSIZE>, COSTMATRIXSIZE> graph;


	for(int i=0;i<COSTMATRIXSIZE;i++) {
		graph[routerID][i]=costMatrix[i];
	}

	while (!myqueue.empty()) {
		subPacket temp=myqueue.front();
		myqueue.pop();
		int node=temp.myNodeID;

		for(int i=0;i<COSTMATRIXSIZE;i++) {
			graph[node][i]=temp.myNeigh[i];
		}
	}

	int dist[COSTMATRIXSIZE];
	bool sptSet[COSTMATRIXSIZE];
	int parent[COSTMATRIXSIZE];

	parent[routerID]=-1;
	for (int i = 0; i < COSTMATRIXSIZE; i++) {
		dist[i] = INT_MAX;
		sptSet[i] = false;
	}

	dist[routerID] = 0;

	for (int i=0; i<COSTMATRIXSIZE-1;i++) {

		int u = minDistance(dist, sptSet);
		sptSet[u] = true;

		for (int v = 0; v < COSTMATRIXSIZE; v++) {
			if (!sptSet[v] && graph[u][v]!=1000 && dist[u] != INT_MAX && dist[u]+graph[u][v] < dist[v]) {
				dist[v] = dist[u] + graph[u][v];
				parent[v]=u;
			}
		}
	}

	printSolution(dist, COSTMATRIXSIZE, parent);
	cout<<endl;
}


void LinkState(int myPort) {

	cout<<"----------- LINK STATE -------------\n";
	cout<<"::::::::::::::::::::::::::::::::::::\n";

	ifstream input;
  	input.open("listPorts.txt");

  	int numPorts, portnum;
  	input>>numPorts;
  	for(int i=0;i<numPorts;i++) {
  		input>>portnum;
  		if(portnum!=myPort)
  			listOfPort.push_back(portnum);
  	}

	struct sockaddr_in server,client;
	int sockaddr_len=sizeof(struct sockaddr_in);
	
	if((sock=socket(AF_INET, SOCK_DGRAM, 0))==-1) {
		perror("socket: ");
		exit(-1);
	}

	int broadcastEnable = 1;
    int ret = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
    if (ret) {
		perror("setsockopt");
        exit(1);
    }

	server.sin_family = AF_INET;
	server.sin_port=htons(myPort);
	server.sin_addr.s_addr = inet_addr("0.0.0.0");
	bzero(&server.sin_zero,8);

	if(bind(sock, (struct sockaddr *)&server, sockaddr_len)==-1) {
		perror("Bind");
		exit(-1);
	}

	cout<<"My IP and Port are "<<inet_ntoa(server.sin_addr)<<":"<<ntohs(server.sin_port)<<endl;
	
	thread myThread1 (receiveFunc);
	bool answer;
	cout<<"Start updating tables? (0/1) ";
	cin>>answer;

	thread myThread2 (sendFunc, Ports, costMatrix);

	myThread1.join();
	myThread2.join();

	distance(costMatrix);

	close(sock);
	return;

}