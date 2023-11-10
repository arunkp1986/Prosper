#include<stdio.h>
#include<stdlib.h>

int main(int argc, char* argv[]){
    if(argc<2){
        printf("pass gemos out and err file paths\n");
	exit(-1);
    }
    printf("%d,name: %s,param1: %s,param2: %s\n",argc,argv[0],argv[1],argv[2]);
    return 0;
}
