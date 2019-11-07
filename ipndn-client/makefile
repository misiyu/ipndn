cli : 
	g++ client.cpp -o client -std=c++14 -lpthread -ljsoncpp
ndn_cli :
	g++ ndn_client.cpp rqueue.cpp -o ndn_client -std=c++14 -lndn-cxx -lboost_system -lpthread -ljsoncpp

lcndn : 
	g++ lclient.cpp -o lcndn -lpthread

rcndn : 
	g++ rclient.cpp -o rcndn -lpthread

clean :
	rm lcndn rcndn ndn_client output *.mp4
