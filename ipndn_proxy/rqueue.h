#ifndef WGH_RQUEUE_H
#define WGH_RQUEUE_H

#define BUFF_SIZE 8002
#define QUEUE_SIZE 1000
#define TIMEOUT_THRESH 15

#include <pthread.h>
#include <cstring>
#include <unistd.h>
#include <iostream>

using namespace std;

class RQueue{
	private : 
		char queue[QUEUE_SIZE][BUFF_SIZE] ;
		bool has_data[QUEUE_SIZE];
		int cwnd_left ;
		int cwnd_right ;
		int cwnd_left_indicate ;
		int head ;
		int thresh ;
		int cwnd ;
		int recv_count ;
		int time_out ;
		pthread_mutex_t queue_mutex ;

	public :
		RQueue();
		~RQueue();
		bool is_full();
		bool is_empty();
		bool add(char * buff, int seg_no);
		bool push(char * buff);
		bool pop(char * buff);
		int get_cwnd();
		int get_waiting();
		void set_cwnd(int cmd);
		void set_thresh(int cmd);

};

#endif 
