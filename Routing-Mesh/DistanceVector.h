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
#include <time.h>
#define COSTMATRIXSIZE 2
#define MAX_subPacket 1024

using namespace std;

std::map<pair<string, int>, int> neighID;
std::map<int, pair<string, int>> router;
std::array<int, COSTMATRIXSIZE> costMatrix;
std::map<int, int> Ports;
int sockConn, routerID, sock;
std::vector<std::vector<int> > path(COSTMATRIXSIZE);
std::queue<struct subPacket> myqueue;
mutex mtx;
bool timerOver;

int parentDV[COSTMATRIXSIZE];

//Packet
struct subPacket{

	int myNodeID;
	int updatedFrom;
	std::array<int, COSTMATRIXSIZE> myNeigh;

};

class timer {
	private:
		unsigned long begTime;
	public:
		void start() {
			begTime = clock();
		}

		unsigned long elapsedTime() {
			return ((unsigned long) clock() - begTime) / CLOCKS_PER_SEC;
		}

		bool isTimeout(unsigned long seconds) {
			return seconds >= elapsedTime();
		}
};


// Send Function
void sendFunc(int update) {

	for (std::map<pair<string, int>,int>::iterator it=neighID.begin(); it!=neighID.end(); ++it) {

		pair<string, int> p = it->first;
		struct sockaddr_in remoteaddr;
		remoteaddr.sin_family = AF_INET;
		remoteaddr.sin_addr.s_addr = inet_addr((p.first).c_str());
		remoteaddr.sin_port = htons(p.second);
		socklen_t addr_size = sizeof(remoteaddr);
		struct subPacket *inp=(struct subPacket*)malloc(sizeof(struct subPacket));
		inp->myNodeID=routerID;
		inp->myNeigh=costMatrix;
		inp->updatedFrom=update;
    	sendto(sock,inp,sizeof(*inp),0,(struct sockaddr *)&remoteaddr,addr_size);

	}

}


//Function to update the table
void updateMatrix() {

	atomic<bool> updated;
	unsigned long seconds = 30;
	timer t;
	t.start();

	while(true) {

		updated=false;

		if(t.elapsedTime() >= seconds) {
			break;
		}

		mtx.lock();
		if(myqueue.size()!=0) {

			subPacket temp = myqueue.front();
			myqueue.pop();
			for(int i=0;i<COSTMATRIXSIZE;i++) {
				if (costMatrix[i]>temp.myNeigh[i]+costMatrix[temp.myNodeID]) {
					costMatrix[i]=temp.myNeigh[i]+costMatrix[temp.myNodeID];
					parentDV[i]=temp.updatedFrom;
					updated=true;
				}
			}

			cout<<"Received new table from "<<temp.myNodeID<<endl;
			cout<<".......Updated Matrix......\n";
			for(int i=0;i<COSTMATRIXSIZE;i++)
				cout<<i<<"->"<<costMatrix[i]<<"\n";
			cout<<"---------------------------\n";
			if(updated) {
				sendFunc(temp.myNodeID);
			}
		}
		mtx.unlock();
	}

	timerOver=true;
	sendFunc(0);

}

//Function to receive messages
void receiveFunc() {

	char buffer[1024];
	int nBytes;

	struct sockaddr_in remoteaddr;
	socklen_t addr_size = sizeof(remoteaddr);

	unsigned long seconds = 20;
	timer t;
	t.start();
	// cout << "timer started . . ." << endl;

	while(true) {

		if(t.elapsedTime() >= seconds) {
			// cout<<"Out of recv\n";
			break;
		}

    	nBytes = recvfrom(sock,buffer,1024,0,(struct sockaddr *)&remoteaddr, &addr_size);

    	if(timerOver){
    		// cout<<"Out of recv\n";
    		break;
    	}
        struct subPacket *inp=(struct subPacket*)malloc(sizeof(struct subPacket));
    	inp=(struct subPacket*)buffer;

    	mtx.lock();
    	myqueue.push(*inp);
    	mtx.unlock();
	}
}


void printPath(int parentDV[], int j, int idx)
{
    if (parentDV[j] == - 1)
        return;
    printPath(parentDV, parentDV[j],idx);
    printf("%d ", j);
    path[idx].push_back(j);
}

int printSolution(int dist[], int n, int parentDV[]) {
	cout<<"-------- DISTANCE VECTOR -----------\n";
	cout<<"::::::::::::::::::::::::::::::::::::\n";
    int src = routerID;
    printf("Vertex\t Distance\tPath");
    for (int i = 0; i < COSTMATRIXSIZE; i++)
    {
    	if(i==routerID)
    		continue;
        printf("\n%d -> %d \t\t %d\t\t%d ",src, i, dist[i], src);
        printPath(parentDV, i, i);
    }
}


void DistanceVector(int myPort) {

	timerOver=false;

	struct sockaddr_in server,client;
	int sockaddr_len=sizeof(struct sockaddr_in);
	int subPacket_len;
	char subPacket[MAX_subPacket];

	if((sock=socket(AF_INET, SOCK_DGRAM, 0))==-1) {
		perror("socket: ");
		exit(-1);
	}

	server.sin_family = AF_INET;
	server.sin_port=htons(myPort);
	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	bzero(&server.sin_zero,8);


	if(bind(sock, (struct sockaddr *)&server, sockaddr_len)==-1) {
		perror("Bind");
		exit(-1);
	}

	cout<<"My IP and Port are "<<inet_ntoa(server.sin_addr)<<":"<<ntohs(server.sin_port)<<endl;

	bool answer;
	cout<<"Start updating tables? ";
	cin>>answer;
	thread myThread1 (receiveFunc);
	thread myThread2 (updateMatrix);

	int dist[COSTMATRIXSIZE];

	sendFunc(routerID);
	myThread1.join();
	myThread2.join();

	for(int i=0;i<costMatrix.size();i++) {
		dist[i]=costMatrix[i];
	}
	
	cout<<"parent-----------\n";
	for(int i=0;i<COSTMATRIXSIZE;i++) {
		cout<<parentDV[i]<<" ";
	}
	cout<<endl;
	printSolution(dist, COSTMATRIXSIZE, parentDV);
	cout<<endl;

	close(sock);

}