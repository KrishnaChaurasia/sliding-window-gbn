/*
  Sending frames in multiples of three(SWS) are necessary since if that is not done then at receiver computecrc(frames) will become false and receiver will send NACK instead at the end
*/

#include<iostream>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<netdb.h>
#include<sys/uio.h>
#include<unistd.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/syscall.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<time.h>                          //for sleep()
using namespace std;
#define PORT 5556
#define BACKLOG 3

void computeCrc();
void corrupt();
void corruptOrNot();
void frameDropOrNotfn();
void slidewindow();

int frameDropOrNot;
int size = 40;                            //32+8 frame size
int sizep=9;                              //size of CRC polynomial 
char msg[40]={' '};                       //original message read from file and CRC appended
char tmsg[123]={' '};                     //temporary msg to store the frames for resending if NACK is received
char reply[2]={' '};                      //reply received to the sent message(ACK/NACK)
char tempmsg[50]={' '};                   //used in CRC computation
char send_msg[123] = {' '};               //what message to send per rtt; (40+1)*sws={framesize+id}*sws
int tsend_msg[123] = {' '};               //to avoid garbage 
char poly[9]={'1','0','0','0','0','0','1','1','1'};  //crc polynomial
int noOfAttempts = 0;                     //number of attempts made
int sws = 3;                              //assuming sws to be 3
int readsize = 33;                        //size to be read from file 
int fid = 0;                              //frame id 
int initfid = 0;                          //initial fid to know how many more frames to pump out on the window
char initid = ' ';
int append = 0;                           //append from this location in send_msg in case of ACK recieve
int left_ = 0;

int main(){
  srand(time(NULL));
  int fd,fd2,bnd,rcv,s;
  struct sockaddr_in server,client;
  int from, n=1;
  int flag=0;                             //to decide whether to resend the frame or not based on ack or nack
  fd=socket(AF_INET,SOCK_STREAM,0);
  if(fd<0){
    cout<<"Error creating socket\n";
    return 0;
  }
  cout<<"Socket successfully created....\n";
  server.sin_family=AF_INET;
  server.sin_port=htons(PORT);
  server.sin_addr.s_addr=INADDR_ANY; 
  
  bnd=bind(fd,(struct sockaddr*)&server,sizeof(server));
  if(bnd<0){
    cout<<"Error binding socket\n";
    return 0;
  }
  cout<<"Binding successful....\n";
  if(listen(fd,BACKLOG)<0){
    cout<<"Error listening\n";
    return 0;
  }
  cout<<"Server is listening....\n";
  socklen_t len=sizeof(client);
  fd2=accept(fd,(struct sockaddr*)&client,&len);
  if(fd2<0){
    cout<<"Error accepting\n";
    return 0;
  }
  cout<<"Accept successful....\n"<<endl;
  
  from=open("input.txt",O_RDONLY);
  if(from<0){
    cout<<"Error opening file\n";
    return 0;
  }
  flag = 1;                             //set flag to read first frame
  left_ = sws;                          //initially slide the window equal to size of the window
  fid = 0;
  int finish = 0;                       //to end the process based on the strategy that if ACK was received and no more frames are to be read then terminate the process
  
  while(1){
    noOfAttempts++;
    if(flag==1){                        //read only if ACK is received
      while(left_!=0){                  //initially read 3 frames as sws=3 
	left_--;
	n=read(from,msg,readsize);
	if(msg[0] !='0' && msg[0] !='1'){
	  if(left_==2)
	    finish++;
	  break;                        //to remove the xtra last msg
	}
	
	cout<<"Message read : "<<msg<<endl;
	for(int i=0;i<size-sizep+1;i++){
	  tempmsg[i]=msg[i];
	}
	for(int i=size-sizep+1;i<size;i++) tempmsg[i]='0';
	computeCrc();
	cout<<"Message after crc : "<<msg<<endl<<endl;
	int i = 0;
	while(msg[i]!='\0'){
	  send_msg[append++] = msg[i++];
	}
	
	if(fid == 0){
	  send_msg[append++] = '0';
	  fid++;
	}
	else if(fid == 1){
	  send_msg[append++] = '1';
	  fid++;
	}
	else if(fid == 2){
	  send_msg[append++] = '2';
	  fid++;
	}
	else{
	  send_msg[append++] = '3';
	  fid = 0;
	}
      }
      /*//done to avoid extra garbage
	int i = 0; append = 0;
	while(append<123){
	tsend_msg[append++]=send_msg[i++];
	}*/
      if(finish == 2){
	close(fd);
	close(fd2);
	cout<<"Socket closed successfully"<<endl;
	cout<<"\nNumber of total attempts made : "<<noOfAttempts<<endl<<endl;
	return 0;
      }
      for(int i = 0 ; i<123 ; i++)
	tmsg[i] = send_msg[i];
      
      frameDropOrNotfn();
      if(frameDropOrNot==0){
	corruptOrNot();
	s=send(fd2,send_msg,sizeof(send_msg),0);
	cout<<"Message sent : "<<send_msg<<endl;
	initid = send_msg[40];
        
	//update send_msg with tmsg after sending every time since it gets corrupted and again the corrupted msg is sent
	int i = 0;
	while(tmsg[i]!='\0')
	  send_msg[i]=tmsg[i++];
	if(initid == '0')
	  initfid = 0;
	else if(initid == '1')
	  initfid = 1;
	else if(initid == '2')
	  initfid = 2;
	else
	  initfid = 3;
	
	while(1){
	  fcntl(fd, F_SETFL, O_NONBLOCK);
	  sleep(1.5);
	  rcv=recv(fd2,reply,sizeof(reply),MSG_DONTWAIT);
	  cout<<"Reply : "<<reply<<endl<<endl;
	  if(reply[0]=='A'){
	    if(reply[1]=='0')
	      fid = 0;
	    else if(reply[1]=='1')
	      fid = 1;
	    else if(reply[1]=='2')
	      fid = 2;
	    else fid = 3;
	    finish = 1;
	    
	    //slide the window equal to the size of the sender's window when ACK
	    left_ = 3;
	    slidewindow();
	    flag=1;
	    for(int i=0;i<2;i++)
	      reply[i]=' ';
	    break;
	  }
	  else if(reply[0]=='N'){
	    if(initfid == 0){
	      if(reply[1] == '0')
		left_ = 0;
	      if(reply[1] == '1')
		left_ = 1;
	      if(reply[1] == '2')
		left_ = 2;
	    }
	    if(initfid == 1){
	      if(reply[1] == '1')
		left_ = 0;
	      if(reply[1] == '2')
		left_ = 1;
	      if(reply[1] == '3')
		left_ = 2;
	    }
	    if(initfid == 2){
	      if(reply[1] == '2')
		left_ = 0;
	      if(reply[1] == '3')
		left_ = 1;
	      if(reply[1] == '0')
		left_ = 2;
	    }
	    if(initfid == 3){
	      if(reply[1] == '3')
		left_ = 0;
	      if(reply[1] == '0')
		left_ = 1;
	      if(reply[1] == '1')
		left_ = 2;
	    }
	    slidewindow();
	    
	    if(initfid == 0)
	      fid = 3;
	    else if(initfid == 1)
	      fid = 0;
	    else if(initfid == 2)
	      fid = 1;
	    else
	      fid = 2;
	    
	    flag = 1;
	    for(int i=0;i<2;i++)
	      reply[i]=' ';
	    break;
	  }
	  else{
	    flag=-1;
	    for(int i=0;i<2;i++)
	      reply[i]=' ';
	    break;
	  }
	}
	if(flag==-1){
	  cout<<"Reply dropped\n";
	  sleep(2);
	  cout<<"Timeout happened"<<endl;
	  flag=0;
	  continue;
	}
      }
      //frame dropped while sending the newly read frame
      else {
	sleep(2);
	cout<<"\n\nTimeout happened"<<endl;
	flag=0;
	continue; 
      }
    }
    //NACK received part
    else {
      cout<<endl;
      frameDropOrNotfn();
      if(frameDropOrNot==0){
        int i = 0; 
	while(tmsg[i]!='\0')
	  send_msg[i]=tmsg[i++];
	cout<<"Previous frames read from file : "<<send_msg<<endl;
	corruptOrNot();
	s=send(fd2,send_msg,sizeof(send_msg),0);
	cout<<"Message sent : "<<send_msg<<endl;
	initid = send_msg[40];
	
	//update send_msg with tmsg after sending every time since it gets corrupted and again the corrupted msg is sent
	i = 0;
	while(tmsg[i]!='\0')
	  send_msg[i]=tmsg[i++];
	if(initid == '0')
	  initfid = 0;
	else if(initid == '1')
	  initfid = 1;
	else if(initid == '2')
	  initfid = 2;
	else
	  initfid = 3;
	
	while(1){
	  fcntl(fd, F_SETFL, O_NONBLOCK);
	  sleep(1.5);
	  rcv=recv(fd2,reply,sizeof(reply),MSG_DONTWAIT);
	  cout<<"Reply : "<<reply<<endl<<endl;
	  if(reply[0]=='A'){
	    if(reply[1]=='0')
	      fid = 0;
	    else if(reply[1]=='1') 
	      fid = 1;
	    else if(reply[1]=='2')
	      fid = 2;
	    else
	      fid = 3;
	    finish = 1;
	    
	    //window can be slided as ACK received and three more frame can be read from the file
	    left_ = 3;
	    slidewindow();
	    flag=1;
	    for(int i=0;i<2;i++)
	      reply[i]=' ';
	    break;
	  }
	  else if(reply[0]=='N'){
	    if(initfid == 0){
	      if(reply[1] == '0')
		left_ = 0;
	      if(reply[1] == '1')
		left_ = 1;
	      if(reply[1] == '2')
		left_ = 2;
	    }
	    if(initfid == 1){
	      if(reply[1] == '1')
		left_ = 0;
	      if(reply[1] == '2')
		left_ = 1;
	      if(reply[1] == '3')
		left_ = 2;
	    }
	    if(initfid == 2){
	      if(reply[1] == '2')
		left_ = 0;
	      if(reply[1] == '3')
		left_ = 1;
	      if(reply[1] == '0')
		left_ = 2;
	    }
	    if(initfid == 3){
	      if(reply[1] == '3')
		left_ = 0;
	      if(reply[1] == '0')
		left_ = 1;
	      if(reply[1] == '1')
		left_ = 2;
	    }
	    slidewindow();
	    if(initfid == 0)
	      fid = 3;
	    else if(initfid == 1)
	      fid = 0;
	    else if(initfid == 2)
	      fid = 1;
	    else
	      fid = 2;
	    
	    flag = 1;
	    for(int i=0;i<2;i++)
	      reply[i]=' ';
	    break;
	  }
	  else {
	    flag=-1;
	    cout<<"Reply Dropped\n";
	    sleep(2);
	    cout<<"Timeout happened"<<endl;
	    for(int i=0;i<2;i++)
	      reply[i]=' ';
	    break;
	  }
	}
	if(flag == -1){
	  flag=0;
	  continue;
	}
      } //frame dropped while sending the previous frame
      else {
	sleep(2);
	cout<<"\n\nTimeout happened"<<endl;
	flag=0;
	continue;
      }
    }
    for(int i=0;i<32;i++){
      msg[i]=' ';
      tempmsg[i]=' ';
    }
  }
  close(fd2);
  close(fd);
  shutdown(fd,0);
  cout<<"\nSocket closed\n";
  cout<<"\nNumber of total attempts made : "<<noOfAttempts<<endl<<endl;
  return 0;
}

void computeCrc(){
  int j=0; int ctr=0; int l=0;
  int count=0;
  int sz=size;
  while(sz>=sizep){
    ++count;l=0;j=0;ctr=0;
    for(int i=0;i<sizep;i++){
      if(tempmsg[j]==poly[i]){
	if(ctr!=0){
	  ++ctr;
	  tempmsg[l++]='0';       //push '0' in temporary array
	}
      }
      else{
	tempmsg[l++]='1';	  //push '1' in temporary array 
	++ctr;
      }
      j++;
    }
    for(int i=sizep;i<=sz;i++){
      tempmsg[l++]=tempmsg[i];
    }
    sz=l-1;
  }
  //  cout<<"tempmsg : ";
  //for(int i=0;i<sz;i++) cout<<tempmsg[i];cout<<endl;
  if(sz<sizep-1){
    int t=sizep-sz-1;
    int i=size-sizep+1;
    while(t>0){
      msg[i++]='0';
      t--;
    }
    j=0;
    while(tempmsg[j]!='\0')  msg[i++]=tempmsg[j++];
  }
  else{
    int i=size-sizep+1;
    int j=0;
    while(tempmsg[j]!='\0')  msg[i++]=tempmsg[j++];
  }
}

void corruptOrNot(){
  int flag=0;
  flag=rand()%40;
  if(flag%2==0){
    cout<<"Message corrupted\n";
    corrupt();
  }
  else
    cout<<"Message not corrupted\n";
}

void corrupt(){
  int check[123]={0};
  check[40]=1; check[81]=1; check[122]=1; //to avoid frame no. to get corrupted
  int corruptbits=rand()%10;
  cout<<"Number of bits to corrupt :"<<corruptbits<<endl;
  while(corruptbits>0){
    int bitno=rand()%123;
    if(check[bitno]==0){
      if(send_msg[bitno]=='1')
	send_msg[bitno]='0';
      else
	send_msg[bitno]='1';
      check[bitno]=1;
      corruptbits--;
    }
  }
}

void frameDropOrNotfn(){
  int flag=0;
  flag=rand()%40;
  if(flag%2==0){
    cout<<"Frame Dropped\n";
    frameDropOrNot=1;
  }
  else{
    frameDropOrNot=0;
    cout<<"Frame not dropped\n";
  }
}

void slidewindow(){
  int i = 41*left_;
  int j = 0;
  while(send_msg[i] != '\0' && send_msg[i] != ' '){
    send_msg[j++]=send_msg[i++];
  }
  append = j;
  for(int k=append; k<123; k++)
    send_msg[k]=' ';
}
