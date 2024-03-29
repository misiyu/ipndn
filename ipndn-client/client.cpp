#include <iostream>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <cstdlib>
#include <fstream>
#include <jsoncpp/json/json.h>
#include <chrono>

#define BUFF_SIZE 8002
#define PLAY_THRESH 1000
#define PORT 9759

using namespace std;

int if_open_player = 0 ;
string proxy_ip = "127.0.0.1" ;
string player_app = "smplayer";

ssize_t readn(int fd, char *buf, int n){
    size_t nleft = n; //还需要读取的字节数
    char *bufptr = buf; //指向read函数当前存放数据的位置
    ssize_t  nread;
    while(nleft > 0){
        if((nread = read(fd, bufptr, n)) < 0){
            if(errno == EINTR){ //遇到中断
                continue;
            }
            else            // 其他错误
                return -1;
        }
        else if(nread == 0){ // 遇到EOF
            break;
        }
 
        nleft -= nread;
        bufptr += nread;
    }
    return (n - nleft);
}

void * open_player(void * val){
	char * file_path = (char*)val ;
	//string appname = "smplayer";
	string cmd = player_app +" "+file_path ;
	system(cmd.data());
	return NULL ;
}

void load_conf(){
	string file_path = "./client_conf.json";
	Json::Reader reader ;
	Json::Value root ;
	ifstream in(file_path.data(),ios::binary);
	if(!in.is_open()){
		cout << "can not open file " << file_path << endl ;
		exit(1);
	}
	reader.parse(in,root);
	in.close();
	proxy_ip = root["proxy_ip"].asString() ;
	cout << "proxy_ip = " << proxy_ip << endl ;
	if_open_player = root["openplayer"].asInt() ;
	cout << "if_open_player = " << if_open_player << endl ;
	player_app = root["player"].asString();
	cout << "player = " << player_app << endl ;
}


int main(int argc , char ** argv)
{
	string interest_name = "/localhost/nfd/testApp/003.mkv";
	load_conf();
	if(argc < 2) {
		cout << "Usage : ./client [ndn source name]" << endl ;
		cout << "example : ./client /test/aaa/testApp/003.mkv" << endl ;
		exit(1);
	}else{
		interest_name = argv[1];
	}
	int sockfd ;
	struct sockaddr_in server_addr ;
	int port = PORT ;
	std::chrono::time_point<std::chrono::steady_clock> start , end ;
	std::chrono::duration<double,std::milli> elapsed_milliseconds ;
 
	start = std::chrono::steady_clock::now();



	bzero(&server_addr , sizeof(server_addr));
	server_addr.sin_family = AF_INET ;
	server_addr.sin_addr.s_addr = inet_addr(proxy_ip.data());
	server_addr.sin_port = htons(port);

	sockfd = socket(AF_INET , SOCK_STREAM , 0 );
	connect(sockfd , (struct sockaddr *)&server_addr , sizeof(server_addr));

	write(sockfd,interest_name.data(),interest_name.length());
	int tmp = interest_name.find_last_of('/');
	string file_out_name = interest_name.substr(tmp+1 , interest_name.length()-tmp-1);
	FILE * file = fopen(file_out_name.data(),"wb");
	char recv_buff[BUFF_SIZE];
	int recv_count = 0 ;
	int recv_num = 0;


	while((recv_num = readn(sockfd,recv_buff,BUFF_SIZE))){
		write(sockfd,"o",1);
		int16_t valid_num = 0 ;
		memcpy(&valid_num,recv_buff+BUFF_SIZE-2,sizeof(int16_t));
		if(valid_num <= 0 ) continue ;
		recv_count ++ ;
		cout << recv_count << " : valid_num = " << valid_num << "  recv_num = "<<recv_num << endl ;
		fwrite(recv_buff,sizeof(char),valid_num,file);
		if(recv_count % PLAY_THRESH == 0) break ;
	}
	fclose(file);
	pthread_t tid ;
	if(if_open_player ) pthread_create(&tid,NULL,open_player,(void*)(file_out_name.data()));

	while(1)
	{
		file = fopen(file_out_name.data(),"ab+");
		fseek(file,0,SEEK_END);

		while((recv_num = readn(sockfd,recv_buff,BUFF_SIZE))){
			write(sockfd,"o",1);
			int16_t valid_num = 0 ;
			memcpy(&valid_num,recv_buff+BUFF_SIZE-2,sizeof(int16_t));
			if(valid_num <= 0 ) continue ;
			recv_count ++ ;
			cout << recv_count << " : valid_num = " << valid_num << "  recv_num = "<<recv_num << endl ;
			fwrite(recv_buff,sizeof(char),valid_num,file);
			if(recv_count % 2000 == 0) break ;
		}
		if(recv_num == 0) break ;
		fclose(file);
		cout << "write buff" << endl ;
	}
	end = std::chrono::steady_clock::now();
	elapsed_milliseconds = end - start ;
	double duration_time = elapsed_milliseconds.count()/1000 ;
	printf("time :  %lf s\n",duration_time);
	double bit_Byte = 8.0;
	double rate  = BUFF_SIZE*bit_Byte*recv_count/duration_time ;
	if(rate > 1000000) printf("rate :  %lf Mbps\n", rate/1000000);
	else if(rate > 1000) printf("rate :  %lf Mbps\n", rate/1000);
	else printf("rate :  %lf bps\n", rate);

	close(sockfd);
	fclose(file);
	if(if_open_player) pthread_join(tid,NULL);

	return 0;
}
