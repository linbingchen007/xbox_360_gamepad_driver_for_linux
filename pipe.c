#include <stdio.h>
#include <unistd.h>
#include <linux/input.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <memory.h>
#define MAXBUFFERLEN 300
char buf[MAXBUFFERLEN];
void *TheadingCheckWindow(){
	while(1){
	usleep(200000);
	FILE * fp = popen("xprop -id $(xprop -root 32x '\t$0' _NET_ACTIVE_WINDOW | cut -f 2) WM_CLASS","r");
	while(fgets(buf,sizeof(buf),fp)){
		printf("%s",buf);
	}
	//puts(buf);
	pclose(fp);
	}
	return 0;
}