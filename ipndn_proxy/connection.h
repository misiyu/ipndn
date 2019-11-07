#ifndef WGH_CONNECTION_H
#define WGH_CONNECTION_H
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ndn-cxx/face.hpp>
#include <errno.h>
#include <exception>

#include <iostream>
#include "rqueue.h"

#define BUFF_SIZE 8002
#define ALIVE_R 2000
using namespace ndn ;
using namespace std ;

class Connection : noncopyable{

	private :
		pthread_t tid , socket_thread  ;
		int sockfd ;
		string interest_name ;
		string interest_data_prefix ;
		int file_no ;
		int segment_num ;
		int get_segment_count ;
		int send_interest_count ;
		int send_seg_count ;
		Face m_face ;
		RQueue m_rqueue ;

		void onData(const Interest& interest , const Data& data);
		void onData1(const Interest& interest , const Data& data);
		void onNack(const Interest& interest, const lp::Nack& nack);
		void onTimeout(const Interest& interest);
		void onTimeout1(const Interest& interest);

	public :
		Connection(int sockfd);
		void start();

		static void * run(void * para){

			Connection * _this = (Connection*)para ;
			char buff[1000];
			int len = recv((_this->sockfd), buff,1000,0); 
			buff[len] = '\0';
			if(len <= 1) { 
				close(_this->sockfd);
				return NULL ;
			}
			_this->interest_name = buff ;

			int tmp1 = _this->interest_name.find_last_of('/');
			string str_tmp = _this->interest_name.substr(0,tmp1);
			tmp1 = str_tmp.find_last_of('/');
			_this->interest_data_prefix = str_tmp.substr(0,tmp1+1)+"data";

			Interest interest(Name(_this->interest_name.data()));
			interest.setInterestLifetime(2_s);
			_this->m_face.expressInterest(interest,
					bind(&Connection::onData,_this,_1,_2),
					bind(&Connection::onNack,_this,_1,_2),
					bind(&Connection::onTimeout,_this,_1));
			cout << "express interest : " << interest.getName() << endl ;

			_this->m_face.processEvents(time::milliseconds::zero(),true);
			cout << "face process end !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl ;

			return NULL;
		}

		static ssize_t writen(int fd, const char *vptr, size_t n)
		{
			size_t nleft;
			ssize_t nwritten;
			const char *ptr;
			ptr = vptr;
			nleft = n;
			while (nleft > 0){
				if ( (nwritten = write(fd, ptr, nleft)) <= 0){
					if (nwritten < 0 && errno == EINTR)
						nwritten = 0; //
					else 
						return -1; // error
				}
				nleft -= nwritten;
				ptr += nwritten;
			}
			return n;
		}
		static void * send_data(void * para){
			cout << "send data start " << endl ;
			Connection * _this = (Connection*)para ;
			char buff_ack[1];
			char send_buff[BUFF_SIZE];
			while(1){
				int sleep_count = 0;
				while(_this->m_rqueue.pop(send_buff) == false ) {
					//cout << "no data send" << endl ;
					usleep(10000);
					sleep_count ++ ;
					if(sleep_count % ALIVE_R == 0){
						int16_t alive_sig = -1 ;
						memcpy(send_buff+BUFF_SIZE-2,&alive_sig , sizeof(int16_t));
						writen(_this->sockfd , send_buff , BUFF_SIZE);
						int recv_len = read(_this->sockfd , buff_ack,1 );
						if(recv_len <= 0) { 
							close(_this->sockfd) ;
							_this->m_face.shutdown();
							pthread_exit(NULL);
						}			
					}
				}
				//cout << "send to client " << _this->send_seg_count << endl ;
				int16_t tmp = 0 ;
				memcpy(&tmp, send_buff+BUFF_SIZE-2 , sizeof(int16_t));
				int send_num = 0;
				send_num = writen(_this->sockfd , send_buff , BUFF_SIZE);
				int recv_len = read(_this->sockfd , buff_ack,1 );
				if(recv_len <= 0) { 
					close(_this->sockfd) ;
					pthread_exit(NULL);
				}			
				_this->send_seg_count ++ ;
				if(_this->send_seg_count >= _this->segment_num) {
					close(_this->sockfd);
					break;
				};
			}
		}

};

#endif 
