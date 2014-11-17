import auto							

	











#include <stdio.h>
#define MAXBUFFERLEN 1000
char buf[MAXBUFFERLEN];
int main(){
	int fp = popen("ls -l","r");
	read(fp,buf,sizeof(buf));
	puts(buf);
	pclose(fp);
	return 0
}