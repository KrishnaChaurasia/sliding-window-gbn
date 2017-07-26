#include<iostream>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/uio.h>
#include<unistd.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/syscall.h>

using namespace std;
#define PORT 5556
bool computecrc(char[]);
void replyDropOrNotfn();

int size=40;      //32+8
int sizep=9;
char msg[123]={' '};
char tempmsg0[40]={' '};
char tempmsg1[40]={' '};
char tempmsg2[40]={' '};
char tmsg[123]={' '};
char id0 = ' ',id1 = ' ', id2 = ' ';
char id = ' ';
char poly[9]={'1','0','0','0','0','0','1','1','1'};
char reply[2] = {' ',' '};
int replyDropOrNot, flag;
char recvid[1] = {' '};
int fid = 0;

int main(int argc,char * argv[])
{
  srand(time(NULL));
  int fd;
  struct sockaddr_in server,client;
  struct hostent *hp;
  int con,rec,snd;
  server.sin_family=AF_INET;
  server.sin_addr.s_addr=INADDR_ANY;
  server.sin_port=htons(PORT);
  
  fd=socket(AF_INET,SOCK_STREAM,0);
  if(fd<0){
    cout<<"Error creating socket\n";
    return 0;
  }
  cout<<"Socket Created\n";
  
  hp=gethostbyname(argv[1]);
  bcopy((char*)hp->h_addr,(char*)&server.sin_addr.s_addr,hp->h_length);
  
  con=connect(fd,(struct sockaddr*)&server,sizeof(struct sockaddr));
  if(con<0){
    cout<<"Error connecting\n";
    return 0;
  }
  cout<<"\nConnection has been made\n\n";
  int recvtrue;
  rec=1;
  int received=0;
  int to;
  to=creat("output.txt",0777);
  if(to<0){
    cout<<"Error creating file\n"<<endl;
  }
  int w; 
  while(rec=recv(fd,msg,sizeof(msg),0)){
    received = 0;
    reply[0] = ' '; reply[1] = ' ';
    recvid[0] = ' ';
    if(msg[0]=='0' || msg[0]=='1'){
      received=1;
      cout<<"\nMessage received : "<<msg<<endl;
    }
    else continue;
    if(received){
      id = ' ';
      int start = 0;
      int j = 0;
      for(int i = 0; i < 40; i++){
	tempmsg0[i]=msg[start++];
      }
      id0 = msg[start++];
      //cout<<"tempmsg0 : "<<tempmsg0<<endl;
      //cout<<"id0 : "<<id0<<endl<<endl;
      for(int i = 0; i < 40; i++)
	tempmsg1[i] = msg[start++];
      id1 = msg[start++];
      //cout<<"tempmsg1 : "<<tempmsg1<<endl;
      //cout<<"id1 : "<<id1<<endl<<endl;
      for(int i = 0; i < 40; i++)
	tempmsg2[i] = msg[start++];
      id2 = msg[start++];
      //cout<<"tempmsg2 : "<<tempmsg2<<endl;
      //cout<<"id2 : "<<id2<<endl<<endl;
      
      if(computecrc(tempmsg0) && computecrc(tempmsg1) && computecrc(tempmsg2)){
	reply[0] = 'A';
	if(id2 == '0'){
	  reply[1] = '1';
	  id = '1';
	}
	else if(id2 == '1'){
	  reply[1] = '2';
	  id = '2';
	}
	else if(id2 == '2'){
	  reply[1] = '3';
	  id = '3';
	}
	else{
	  reply[1] = '0';
	  id = '0';
	}
      }
      else if(computecrc(tempmsg0) && computecrc(tempmsg1)){
	reply[0] = 'N';
	reply[1] = id2;
	id = id2;
      }
      else if(computecrc(tempmsg0)){
	reply[0] = 'N';
	reply[1] = id1;
	id = id1;
      }
      else{
	reply[0] = 'N';
	reply[1] = id0;
	id = id0;
      }
      
      replyDropOrNotfn();
      if(replyDropOrNot==0){
	snd=send(fd,reply,sizeof(reply),0);
	cout<<endl<<reply<<" Sent\n";
      }
      else{
	if(flag==1) cout<<"NACK "<<id<<" Dropped\n";
	else{
	  cout<<"ACK "<<id<<" Dropped"<<endl;
	}/*if(id=='0')
	    cout<<"1 Dropped\n";
	    else if(id=='1')
	    cout<<"2 Dropped\n";
	    else if(id=='2')
	    cout<<"3 Dropped\n";
	    else
	    cout<<"0 Dropped\n";	      
	    }*/
      }
    }
    for(int i=0;i<sizeof(msg);i++)
      {
	msg[i]=' ';
      }
    for(int i=0;i<40;i++){
      tempmsg0[i]=' ';
      tempmsg1[i]=' ';
      tempmsg2[i]=' ';
    }
  }
  
  close(fd);
  cout<<"\nSocket closed\n";
  return 0;
}

bool computecrc(char msg[]){
  int j=0; int ctr=0; int l=0;
  int count=0;
  int sz=size;
  int i = 0;
  char tempmsg[40] = {' '};
  while(msg[i]!='\0')
    tempmsg[i]=msg[i++];
  //cout<<"Tempmsg : "<<tempmsg<<endl<<endl;
  
  while(sz>=sizep){
    ++count;l=0;j=0;ctr=0;
    for(int i=0;i<sizep;i++){
      if(tempmsg[j]==poly[i]){
	if(ctr!=0){
	  ++ctr;
	  tempmsg[l++]='0';       //push '0' in temporary array
	}
      }
      else {
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
  
  flag=0;
  for(int i=0;i<sz;i++)
    if(tempmsg[i]=='1'){
      flag++;
      break;
    }
  if(flag == 0)
    return true;
  else
    return false;
}

void replyDropOrNotfn(){     //to determine whether to drop ACK/NACK or not
   int flag=0;
  flag=rand()%40;
  if(flag%2==0){
    replyDropOrNot=1;
  }
  else{
    replyDropOrNot=0;
  }
}
