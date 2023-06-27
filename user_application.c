#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

static struct termios old, current;

void initTermios(void){
	tcgetattr(0, &old);
	current = old;
	current.c_lflag &= ~ICANON;
	current.c_lflag |= ECHO;
	tcsetattr(0, TCSANOW, &current);
}

void resetTermios(void){
	tcsetattr(0, TCSANOW, &old);
}

char getche(void){
	char ch;
	initTermios();
	ch = getchar();
	resetTermios();
	return ch;
}

int main(){
	int fd;
	char data;
	
	fd = open("/dev/keyboard_simulator", O_RDWR);
	if(fd < 0){
		printf("Cannot open device file...\n");
		return 0;
	}

	while((data = getche()) != EOF){
		write(fd, &data, 1);
		//printf("%c", data);
	}

	close(fd);
	return 0;
}
