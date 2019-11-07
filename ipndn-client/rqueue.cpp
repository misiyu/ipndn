#include "rqueue.h"
#include <unistd.h>

RQueue::RQueue(){
	this->cwnd_left = 0 ;
	this->cwnd_right = 0 ;
	this->cwnd_left_indicate = 0;
	this->head = 0 ;
	this->thresh = 50 ;
	this->cwnd = 4 ;
	this->recv_count = 0;
	pthread_mutex_init(&(this->queue_mutex) , NULL);
	for (int i = 0; i < QUEUE_SIZE; i++) {
		this->has_data[i] = false ;
	}
}

RQueue::~RQueue(){
	pthread_mutex_destroy(&(this->queue_mutex));
}

bool RQueue::is_full(){
	bool ret = false ;
	pthread_mutex_lock(&(this->queue_mutex));
	ret = ((this->cwnd_left +1)%QUEUE_SIZE == this->head ) ;
	pthread_mutex_unlock(&(this->queue_mutex));
	return ret ;
}

bool RQueue::is_empty(){
	//cout << this->cwnd_left << ":" << this->head << endl ;
	bool ret = false ;
	pthread_mutex_lock(&(this->queue_mutex));
	ret = (this->head == this->cwnd_left ) ;
	pthread_mutex_unlock(&(this->queue_mutex));
	return ret;
}

bool RQueue::push(char * buff){
	if(this->is_full())	return false ;
	memcpy(this->queue[this->cwnd_left] , buff , BUFF_SIZE);
	pthread_mutex_lock(&(this->queue_mutex));
	this->cwnd_left = (this->cwnd_left + 1) % QUEUE_SIZE ;
	pthread_mutex_unlock(&(this->queue_mutex));
	return true ;
}

bool RQueue::add(char * buff , int seg_no){
	if(seg_no < this->cwnd_left_indicate ) return true ;

	this->recv_count ++ ;
	if(this->recv_count >= this->thresh && this->time_out < 1){
		this->set_cwnd(1);
		this->recv_count -= this->thresh ;
		this->time_out = 0;
	}

	int inset_i = (seg_no-this->cwnd_left_indicate+this->cwnd_left)%QUEUE_SIZE ;
	memcpy(this->queue[inset_i] , buff , BUFF_SIZE);
	pthread_mutex_lock(&(this->queue_mutex));
	this->has_data[inset_i] = true ;
	while(this->has_data[this->cwnd_left] ) {
		if((this->cwnd_left +1)%QUEUE_SIZE == this->head ) break;
		this->cwnd_left = (this->cwnd_left+1)%QUEUE_SIZE ;
		this->cwnd_left_indicate ++ ;
		if(this->cwnd_left + 1 == this->head) break ;
	}
	pthread_mutex_unlock(&(this->queue_mutex));
	return true ;
}

bool RQueue::pop(char * buff){
	if(this->is_empty()) return false ;
	memcpy(buff , this->queue[this->head] , BUFF_SIZE);
	pthread_mutex_lock(&(this->queue_mutex));
	this->has_data[this->head] = false ;
	//cout << "pop head = " <<  this->head << endl ;
	this->head = (this->head + 1) % QUEUE_SIZE ;
	//if(!this->is_empty()) this->head = (this->head + 1) % QUEUE_SIZE ;
	pthread_mutex_unlock(&(this->queue_mutex));
	return true ;
}

int RQueue::get_waiting(){
	int result = 0 ;
	for (int i = this->cwnd_left; i != this->cwnd_right; i = (i+1)%QUEUE_SIZE) {
		if(this->has_data[i] == false) result ++ ;
	}
	return result ;
}

// get the size of free space in cwnd 
int RQueue::get_cwnd(){
	int temp = this->cwnd_left ;
	int cur_cwnd = (this->cwnd_right-temp+QUEUE_SIZE)%QUEUE_SIZE ; 
	// the data size in current cwnd 
	if(cur_cwnd > this->cwnd) return 0 ;
	for (int i = 0; i < this->cwnd; i++) {
		if((temp + 1)%QUEUE_SIZE == this->head) break;
		temp = (temp+1)%QUEUE_SIZE;
	}
	int result  = 0;
	pthread_mutex_lock(&(this->queue_mutex));
	result = (temp-this->cwnd_right+QUEUE_SIZE)%QUEUE_SIZE  ;
	this->cwnd_right = temp ;
	pthread_mutex_unlock(&(this->queue_mutex));
	if(result < 0) return 0 ;
	return result ;
}

void RQueue::set_cwnd(int cmd){
	this->cwnd += cmd ;
	if(this->cwnd <= 0 ) this->cwnd = 1 ;
	cout << "cwnd = " << cwnd << " thresh = " << thresh << endl ; 
}

void RQueue::set_thresh(int cmd){
	this->time_out ++ ;
	if(this->time_out >= 1){
		this->cwnd -- ;
		this->time_out = 0;
		this->recv_count = 0;
	}
	cout << "cwnd = " << cwnd << " thresh = " << thresh << endl ; 
}
