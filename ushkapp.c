#include <stdio.h>
#include <unistd.h>
#include <linux/input.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <memory.h>
#define SENSETIVE 3
#define FRESTIME 1000
#define REPTIME 10000
#define PACKETLEN sizeof(tmpevent)
unsigned char buf[32];
unsigned char tmpbuf[32];
unsigned char cint[4];
char strline[100];
int * pint;
  struct input_event eventlx, event_end ,evently,clkevet1,clkevet2,clkevet3,tmpevent;
	pthread_t pthx,pthy,pthrep;
  int i ,lxval ,j ,fd,fd2,lyval,fd3;
void *do_x( void *arg){
	while(1){
		usleep(FRESTIME);
		memset(&eventlx,0,sizeof(eventlx));
		gettimeofday(&eventlx.time,NULL);
		eventlx.type = EV_REL;
		eventlx.code = REL_X;
		if(tmpevent.code == 0x00 && tmpevent.type == 0x02){
		
			lxval = (*pint)/SENSETIVE;
			eventlx.value = tmpevent.value/SENSETIVE ;
			
			write(fd2,&eventlx,sizeof(eventlx));
			
		} 
		
	}
	return NULL;
}
void *do_rep( void *arg){
	while(1){
		usleep(FRESTIME);		
		memset(&event_end,0,sizeof(event_end));
		gettimeofday(&event_end.time,NULL);
		event_end.type = EV_SYN;
		event_end.code = SYN_REPORT ;
		event_end.value = 0;
		
		write(fd2,&event_end,sizeof(event_end));			
	
		
	}
	return NULL;
}
void *do_y( void *arg){
	while(1){
		usleep(FRESTIME);
		memset(&evently,0,sizeof(evently));
		gettimeofday(&evently.time,NULL);
		evently.type = EV_REL;
		evently.code = REL_Y;
		if(tmpevent.code == 0x01 && tmpevent.type == 0x02){
			//printf("seg thread y\n");
			lyval = (*pint)/SENSETIVE;
			evently.value = tmpevent.value /SENSETIVE;
			
			write(fd2,&evently,sizeof(evently));
			
		} 
		
	}
	return NULL;
}
void do_click(){
	while(1){
		if(buf[10] == 0x33 && buf[8] == 0x01 && buf[12] == 0x01){
			memset(&clkevet1,0,sizeof(clkevet1));
			memset(&clkevet2,0,sizeof(clkevet2));
			memset(&clkevet3,0,sizeof(clkevet3));
			gettimeofday(&clkevet1.time,NULL);
			clkevet1.type = 4;
			clkevet1.code = 4;
			clkevet1.value = 589825;
			clkevet2.time = clkevet1.time;
			clkevet3.time = clkevet1.time;
			clkevet2.type = 1;
			clkevet2.code = 272;
			clkevet2.value = 1;
       		clkevet3.type = 0;
			clkevet3.code = 0;
			clkevet3.value = 0;
			write(fd2,&clkevet1,sizeof(clkevet1));
			write(fd2,&clkevet2,sizeof(clkevet1));
			write(fd2,&clkevet3,sizeof(clkevet1));
		} 
	}
}
int main() {
  int fst=1;
  fd = open("/dev/input/event13", O_RDWR);
  fd2 = open("/dev/input/event4", O_RDWR);
  fd3 = open("/proc/bus/input/devices",O_RDWR);
 while(read(fd3,&strline,sizeof(strline)-1)){
	strline[sizeof(strline)-1] = 0;
	printf("%s",strline);
 }
	printf("read finished!!\n");
	memset(buf,0,sizeof(buf));
	printf("%d\n",fd2);
   pthread_create(&pthx,NULL,do_x,"processing...");
   pthread_create(&pthy,NULL,do_y,"processing...");
   pthread_create(&pthrep,NULL,do_rep,"processing...");
  write(fd,&buf,20 * sizeof(char));
  while(1){
 	read(fd,&tmpevent,sizeof(tmpevent));

	//printf("debug !!!%d %d\n",buf[10] == 0x00,buf[8] == 0x00) ;
	//printf("debug !!! %d %d %d \n",tmpevent.type,tmpevent.code,tmpevent.value)	;
	
	if (tmpevent.code == 0x00 && tmpevent.type == 0x00) {
		usleep(200);
		continue;
	}
	memcpy(buf,tmpbuf,sizeof(buf));
	for (j = 15 ; j >= 12; j--){
		cint[j-12] = buf[j];
	}
	pint = (int *)cint;

	if(tmpevent.code == 0x13c && tmpevent.type == 0x01 && tmpevent.value ==0x00){
		if (fst) {
			fst = 0;
			continue;
		}
		pthread_cancel(pthx);
		 pthread_cancel(pthrep);
		pthread_cancel(pthy);
		//pthread_join(pthx, NULL);
		//pthread_join(pthy, NULL); 		
		//pthread_join(pthrep, NULL); 
		close(fd2);
		close(fd);
		return 0;
	} else if(tmpevent.code == 0x133 && tmpevent.type == 0x01 ){
			printf("seg 1\n");
			memset(&clkevet1,0,sizeof(clkevet1));
			memset(&clkevet2,0,sizeof(clkevet2));
			memset(&clkevet3,0,sizeof(clkevet3));
			gettimeofday(&clkevet1.time,NULL);
			clkevet1.type = 4;
			clkevet1.code = 4;
			clkevet1.value = 589825;
			clkevet2.time = clkevet1.time;
			clkevet3.time = clkevet1.time;
			clkevet2.type = 1;
			clkevet2.code = 272;				
			clkevet2.value =  tmpevent.value ==0x01 ? 1 : 0;
			printf("seg 1:%d\n",clkevet2.value);
       		clkevet3.type = 0;
			clkevet3.code = 0;
			clkevet3.value = 0;
			write(fd2,&clkevet1,sizeof(clkevet1));
			write(fd2,&clkevet2,sizeof(clkevet1));
			write(fd2,&clkevet3,sizeof(clkevet1));
		} else if(tmpevent.code == 0x130 && tmpevent.type == 0x01 ){
			printf("seg 2\n");
			memset(&clkevet1,0,sizeof(clkevet1));
			memset(&clkevet2,0,sizeof(clkevet2));
			memset(&clkevet3,0,sizeof(clkevet3));
			gettimeofday(&clkevet1.time,NULL);
			clkevet1.type = 4;
			clkevet1.code = 4;
			clkevet1.value = 589826;
			clkevet2.time = clkevet1.time;
			clkevet3.time = clkevet1.time;
			clkevet2.type = 1;
			clkevet2.code = 273;
				
			clkevet2.value =   tmpevent.value ==0x01  ? 1 : 0;
			printf("seg 2:%d\n",clkevet2.value);
       		clkevet3.type = 0;
			clkevet3.code = 0;
			clkevet3.value = 0;
			write(fd2,&clkevet1,sizeof(clkevet1));
			write(fd2,&clkevet2,sizeof(clkevet1));
			write(fd2,&clkevet3,sizeof(clkevet1));
		} 
	
	//for(i =0 ;i<20;i++) printf("0x%02x ",(unsigned char) tmpbuf[i]);
	//if (!(tmpevent.code == 0x00 && tmpevent.type == 0x00) && tmpevent.type != 0x02){
	//printf("0x%02x 0x%02x 0x%02x ",tmpevent.type,tmpevent.code,tmpevent.value)	;
	
	//printf("\n");}
	usleep(200);
	fst =0;
 }
  close(fd2);
  close(fd);
  return 0;
}
