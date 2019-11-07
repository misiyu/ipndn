#include "connection.h"

Connection::Connection(int sockfd){
	this->sockfd = sockfd ;
	this->get_segment_count = 0;
	this->send_seg_count = 0;
	this->send_interest_count = 0;
	this->file_no = 0;
	this->segment_num = 0 ;
	this->interest_data_prefix = "/localhost/nfd/data";
}

void Connection::start(){
	int ret = pthread_create(&(this->tid) , NULL , run , (void*)this);
}
void Connection::onData(const Interest& interest , const Data& data){
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
	if(segment_num <= 0){
		this->m_face.shutdown();
		close(this->sockfd);	
		return ;
	}
	this->segment_num = segment_num ;
	this->file_no = file_no ;

	int ret2 = pthread_create(&(this->socket_thread) , NULL , send_data , (void*)this);
	int cwnd = (this->m_rqueue).get_cwnd();
	for (int i = 0; i < cwnd; i++) {
		stringstream ss ;
		ss << this->interest_data_prefix << "/" << this->file_no<<"-" << this->send_interest_count ;
		this->send_interest_count ++ ;
		string data_segment_name = ss.str();
		Interest interest_d(Name(data_segment_name.data()));
		interest_d.setInterestLifetime(2_s);
		this->m_face.expressInterest(interest_d,
				bind(&Connection::onData1,this,_1,_2),
				bind(&Connection::onNack,this,_1,_2),
				bind(&Connection::onTimeout1,this,_1));
		cout << "expressInterest : " << interest_d.getName() << endl ;
	}

}
void Connection::onData1(const Interest& interest , const Data& data){

	char buff[BUFF_SIZE];
	memcpy(buff,(char*)(data.getContent().value()),BUFF_SIZE);

	this->get_segment_count ++ ;
	cout << this->get_segment_count << ":"<<this->segment_num << ":"<<  endl;
	string data_name = interest.getName().toUri() ;
	//cout << "get data : " << data_name << endl ;
	int temp = data_name.find_last_of('/');
	temp = data_name.find('-',temp);
	stringstream ss ;
	ss << data_name.substr(temp+1, data_name.length()-temp-1);
	int seg_no = 0 ;
	ss >> seg_no ;
	while((this->m_rqueue).add(buff,seg_no) == false) ;
	
	if(this->send_interest_count < this->segment_num){
		int cwnd = (this->m_rqueue).get_cwnd();
		int waiting = (this->m_rqueue).get_waiting() ;
		//cout << "cwnd,wait: " << cwnd <<":"<<waiting  << endl ;
		if(cwnd == 0 &&  waiting == 0  ){
			while((cwnd = (this->m_rqueue).get_cwnd()) == 0) ;
		}
		for (int i = 0; i < cwnd; i++) {
			stringstream ss ;
			ss << this->interest_data_prefix << "/" << this->file_no<<"-" << this->send_interest_count ;
			this->send_interest_count ++ ;
			string data_segment_name = ss.str();
			Interest interest_d(Name(data_segment_name.data()));
			interest_d.setInterestLifetime(2_s);
			this->m_face.expressInterest(interest_d,
					bind(&Connection::onData1,this,_1,_2),
					bind(&Connection::onNack,this,_1,_2),
					bind(&Connection::onTimeout1,this,_1));
			cout << "expressInterest : " << interest_d.getName() << endl ;
		}
	}
	if(this->get_segment_count >= this->segment_num) (this->m_face).shutdown();

}
void Connection::onNack(const Interest& interest, const lp::Nack& nack){
	cout << "no route to " << interest.getName() << endl;
	this->m_face.shutdown();
	close(this->sockfd);	
}
void Connection::onTimeout(const Interest& interest){
	cout << "time out on interest " << interest.getName() << endl ;
	this->m_face.shutdown();
	close(this->sockfd);	
}
void Connection::onTimeout1(const Interest& interest){
	cout << "Time out " << interest.getName() << endl ;
	this->m_rqueue.set_thresh(-2);
	Interest interest_new(interest.getName());
	this->m_face.expressInterest(interest_new,
			bind(&Connection::onData1,this,_1,_2),
			bind(&Connection::onNack,this,_1,_2),
			bind(&Connection::onTimeout1,this,_1));
	cout << "expressInterest : " << interest_new.getName() << endl ;
}

