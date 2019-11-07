#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ndn-cxx/face.hpp>
#include <errno.h>
#include <iostream>
#include <pthread.h>
#include <cstdlib>
#include <jsoncpp/json/json.h>
#include <fstream>
#include <chrono>

#include "rqueue.h"

#define BUFF_SIZE 8002
#define PLAY_THRESH 500
#define ALIVE_R  10


using namespace ndn ;
using namespace std ;

int if_open_player = 0 ;
string proxy_ip = "127.0.0.1" ;
string player_app ;

class NDN_Client : noncopyable{
	private : 
		string interest_name ;
		pthread_t tid , socket_thread  ;
		string interest_data_prefix ;
		int file_no ;
		int segment_num ;
		int get_segment_count ;
		int send_interest_count ;
		int send_seg_count ;
		Face m_face ;
		RQueue m_rqueue ;
		bool player_open ;
		string file_out_name ;

		pthread_t player_pid ; 
		FILE * file_out ;
		bool state ;
		bool isStart ;
	public :
		int get_get_segment_count(){ return this->get_segment_count; }
		NDN_Client(string interest_name){
			this->interest_name = interest_name ;
			this->get_segment_count = 0;
			this->send_seg_count = 0;
			this->send_interest_count = 0;
			this->player_open = false ;
			this->state = true ;
			this->isStart = true ;

			this->load_conf();
			int tmp1 = this->interest_name.find_last_of('/');
			this->file_out_name = this->interest_name.substr(tmp1+1 , 
					this->interest_name.length()-tmp1-1);
			this->file_out = fopen(this->file_out_name.data(),"wb");
			string str_tmp = this->interest_name.substr(0,tmp1);
			tmp1 = str_tmp.find_last_of('/');
			this->interest_data_prefix = str_tmp.substr(0,tmp1+1)+"data";

			Interest interest(Name(this->interest_name.data()));
			interest.setInterestLifetime(2_s);
			this->m_face.expressInterest(interest,
					bind(&NDN_Client::onData,this,_1,_2),
					bind(&NDN_Client::onNack,this,_1,_2),
					bind(&NDN_Client::onTimeout,this,_1));
			cout << "express interest : " << interest.getName() << endl ;
			this->m_face.processEvents();
			if(if_open_player && state == true) 
				pthread_join(this->player_pid,NULL);
			pthread_join(this->socket_thread,NULL);
		}

		static void * send_data(void * para){
			cout << "send data start " << endl ;
			NDN_Client * _this = (NDN_Client*)para ;
			char send_buff[BUFF_SIZE];
			while(1){
				int sleep_count = 0;
				while(_this->m_rqueue.pop(send_buff) == false ) {
					usleep(10000);
					sleep_count ++ ;
				}
				int16_t valid_num = 0 ;
				memcpy(&valid_num, send_buff+BUFF_SIZE-2 , sizeof(int16_t));
				int send_num = 0;
				send_num = fwrite(send_buff , sizeof(char) , valid_num , 
						_this->file_out);
				_this->send_seg_count ++ ;
				cout <<"write to file " << _this->send_seg_count <<":"
					<<_this->segment_num << ":" << valid_num <<endl ;
				int write_p = PLAY_THRESH ;
				if(_this->player_open) write_p = PLAY_THRESH*4 ;
				if(_this->get_segment_count % write_p == 0){
					fclose(_this->file_out);
					_this->file_out = fopen(_this->file_out_name.data(),"ab+");
					fseek(_this->file_out,0,SEEK_END);
					if(_this->player_open == false && if_open_player){
						pthread_create(&_this->player_pid , NULL , open_player ,
								(void*)(_this));
						_this->player_open = true ;
					}
				}
				if(_this->send_seg_count >= _this->segment_num) {
					cout << _this->send_seg_count << " " << "send finish" << endl ;
					fclose(_this->file_out);
					break;
				};
			}
		}
		static void * open_player(void * val){
			NDN_Client * _this = (NDN_Client*)val ;
			const char *file_path = _this->file_out_name.data() ;
			string cmd = player_app +" "+file_path ;
			system(cmd.data());
			_this->state = false ;
			return NULL ;
		}
	private :
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
			if_open_player = root["openplayer"].asInt() ;
			cout << "if_open_player = " << if_open_player << endl ;
			player_app = root["player"].asString();
			cout << "player = " << player_app << endl ;
		}

		// receive the file info ( file size and segment count )
		void onData(const Interest& interest , const Data& data){
			if(this->isStart){
				notify("transfer ok",1) ;
			}
			string data_val = (char*)( data.getContent().value() ) ;
			int tmp1 = data_val.find('-');
			stringstream ss ;
			int file_no ;
			int segment_num ;
			ss << data_val.substr(0,tmp1);
			ss >> file_no ;
			ss.str("");
			ss.clear();
			ss << data_val.substr(tmp1+1,data_val.length() - tmp1-1);
			ss >> segment_num ;
			cout << "file_no=" << file_no << endl ;
			cout << "segment_num=" << segment_num << endl ;
			this->segment_num = segment_num ;
			this->file_no = file_no ;

			int ret2 = pthread_create(&(this->socket_thread) , NULL , send_data ,
					(void*)this);
			int cwnd = (this->m_rqueue).get_cwnd();
			for (int i = 0; i < cwnd; i++) {
				stringstream ss ;
				ss << this->interest_data_prefix << "/" << this->file_no<<"-" 
					<< this->send_interest_count ;
				this->send_interest_count ++ ;
				string data_segment_name = ss.str();
				Interest interest_d(Name(data_segment_name.data()));
				interest_d.setInterestLifetime(2_s);
				this->m_face.expressInterest(interest_d,
						bind(&NDN_Client::onData1,this,_1,_2),
						bind(&NDN_Client::onNack,this,_1,_2),
						bind(&NDN_Client::onTimeout1,this,_1));
				cout << "expressInterest : " << interest_d.getName() << endl ;
			}

		}
		// recieve the data segment 
		void onData1(const Interest& interest , const Data& data){
			if(this->state == false) exit(0) ;

			char buff[BUFF_SIZE];
			memcpy(buff,(char*)(data.getContent().value()),BUFF_SIZE);

			this->get_segment_count ++ ;
			string data_name = interest.getName().toUri() ;
			int temp = data_name.find_last_of('/');
			temp = data_name.find('-',temp);
			stringstream ss ;
			ss << data_name.substr(temp+1, data_name.length()-temp-1);
			int seg_no = 0 ;
			ss >> seg_no ;
			while((this->m_rqueue).add(buff,seg_no) == false) ;

			if(this->send_interest_count < this->segment_num){
				int cwnd = (this->m_rqueue).get_cwnd();
				//cout << "============== " << cwnd << "  ==============" << endl ;
				if(cwnd == 0 && (this->m_rqueue).get_waiting() == 0){
					while((cwnd = (this->m_rqueue).get_cwnd()) == 0) ;
				}
				for (int i = 0; i < cwnd; i++) {
					stringstream ss ;
					ss << this->interest_data_prefix << "/" <<
						this->file_no<<"-" << this->send_interest_count ;
					this->send_interest_count ++ ;
					string data_segment_name = ss.str();
					Interest interest_d(Name(data_segment_name.data()));
					interest_d.setInterestLifetime(2_s);
					this->m_face.expressInterest(interest_d,
							bind(&NDN_Client::onData1,this,_1,_2),
							bind(&NDN_Client::onNack,this,_1,_2),
							bind(&NDN_Client::onTimeout1,this,_1));
				}
			}

		}
		void onNack(const Interest& interest, const lp::Nack& nack){
			cout << "no route to " << interest.getName() << endl ;
			this->state = false ;
			if(this->isStart) notify("no route to "+ interest.getName().toUri(),
					2) ;
			exit(1) ;
		}
		void onTimeout(const Interest& interest){
			cout << "can not find the data :" << interest.getName() << endl ;
			this->state = false ;
			if(this->isStart) notify("can not find data :"+ 
					interest.getName().toUri(), 3) ;
			exit(1) ;
		}
		void onTimeout1(const Interest& interest){
			cout << "Time out " << interest.getName() << endl ;
			this->m_rqueue.set_thresh(-2);
			Interest interest_new(interest.getName());
			this->m_face.expressInterest(interest_new,
					bind(&NDN_Client::onData1,this,_1,_2),
					bind(&NDN_Client::onNack,this,_1,_2),
					bind(&NDN_Client::onTimeout1,this,_1));
			cout << "expressInterest : " << interest_new.getName() << endl ;
		}
		void notify(string msg,int code){
			FILE * msgFile = fopen("./nosign.txt","w") ;
			fprintf(msgFile,"%d %s\n",code ,msg.data()) ;
			fclose(msgFile) ;
			this->isStart = false ;
		}
};

int main(int argc , char ** argv)
{
	std::chrono::time_point<std::chrono::steady_clock> start , end ;
	std::chrono::duration<double,std::milli> elapsed_milliseconds ;
	start = std::chrono::steady_clock::now();

	string interest_name = "/localhost/nfd/testApp/003.mp4";
	if(argc < 2) {
		cout << "Usage : ./client [ndn source name]" << endl ;
		cout << "example : ./ndn_client /test/aaa/testApp/003.mkv" << endl ;
		exit(1);
	}else{
		interest_name = argv[1];
	}
	NDN_Client ndn_client(interest_name.data());

	int recv_count = ndn_client.get_get_segment_count() ;
	end = std::chrono::steady_clock::now();
	elapsed_milliseconds = end - start ;
	double duration_time = elapsed_milliseconds.count()/1000 ;
	printf("time :  %lf s\n",duration_time);
	double bit_Byte = 8.0;
	double rate  = BUFF_SIZE*bit_Byte*recv_count/duration_time ;
	if(rate > 1000000) printf("rate :  %lf Mbps\n", rate/1000000);
	else if(rate > 1000) printf("rate :  %lf Mbps\n", rate/1000);
	else printf("rate :  %lf bps\n", rate);

	return 0;
}
