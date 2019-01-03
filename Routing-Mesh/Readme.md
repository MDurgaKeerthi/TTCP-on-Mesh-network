# Readme

## Compile

* g++ -std=c++11 Main.cpp -lpthread 
* To select Distance Vector/ Link Vector include that corresponding file and uncomment the functipn

## Test

* ./a.out Port1 Port2 Filename
* Port1 - Port at which it receives messages for routing table
* Port2 - Port at which it receives messages for communication
* Filename - List of links
* demo files are shown in Test0
* Example: ./a.out 10000 20000 Test0/router0.txt