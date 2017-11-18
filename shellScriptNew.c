#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>

char buf[4096];
char *arguParse[4096];
int exitCode;
int bufLastByte;
char *redirArray[5];
int stdinRedirFlag=0;
int stdoutRedirFlagA=0;
int stdoutRedirFlagT=0;
int stderrRedirFlagA=0;
int stderrRedirFlagT=0;
int shellScriptFlag=0;
int fd2;
int fileOpenControl;

void readCommand(int i){//Read Command From Stdin (if i=0) Or Shell Script (if i=1) 
	int a;
	int position=0;
	int readControl=1;
	if((i==1)){
		if(fileOpenControl==1){
			if((fd2=open(arguParse[0],O_RDONLY))<0){
				fprintf(stderr,"Open File Error, File %s, %s\n",arguParse[0],strerror(errno));
				i=0;
			}
			fileOpenControl=0;
		}
	}
	while(1){
		int latch=1;
		char input='\0';
		if(position>4095){
			fprintf(stderr,"Argument Overflow(1)\n");
			exit(1);
		}
		if(i==1){
			int scriptEnd;
			scriptEnd=read(fd2,&input,1);
			if(scriptEnd<0){
				fprintf(stderr,"Read Script Error, File %s, %s\n",arguParse[0],strerror(errno));
			}
			if(scriptEnd==0){
				shellScriptFlag=0;
				break;
			}
		}
		else{
			a=scanf("%c",&input);
		}
		if(input=='#'){
			readControl=0;
		}	
		if(a==EOF){
			printf("Exit Shell, Exit Code %d\n",exitCode);
			exit(exitCode);
		}
		if(input=='\n'){
			if(readControl){
				buf[position]='\0';
				bufLastByte=position-1;
				break;
			}
			else{
				readControl=1;
				latch=0;
			}
		}
		if(readControl&&latch){
			buf[position]=input;
			position++;
		}
	}
}

void parseCommand(int j){
	int parsePosition=0;
	if(j==0){
		readCommand(0);
	}
	if(j==1){
		readCommand(1);
	}
	arguParse[parsePosition]=strtok(buf,"\n ");
	while(arguParse[parsePosition]!=NULL){
		parsePosition++;
		if(parsePosition>4095){
			fprintf(stderr,"Argument Overflow(2)\n");
			exit(1);
		}
		arguParse[parsePosition]=strtok(NULL," ");
	}
	if(arguParse[parsePosition-1][0]=='>'){
		if(arguParse[parsePosition-1][1]=='>'){
			stdoutRedirFlagA=1;
		}
		else{stdoutRedirFlagT=1;}
		redirArray[0]=strtok(arguParse[parsePosition-1],">");
		arguParse[parsePosition-1]=NULL;
		arguParse[parsePosition]=NULL;
	}
	else if((arguParse[parsePosition-1][0]=='2')&&(arguParse[parsePosition-1][1]=='>')){
		if(arguParse[parsePosition-1][2]=='>'){
			stderrRedirFlagA=1;
		}
		else{stderrRedirFlagT=1;}
		redirArray[0]=strtok(arguParse[parsePosition-1],"2>");
		arguParse[parsePosition-1]=NULL;
	}
	else if(arguParse[parsePosition-1][0]=='<'){
		stdinRedirFlag=1;
		redirArray[0]=strtok(arguParse[parsePosition-1],"<");
		arguParse[parsePosition-1]=NULL;
	}
	else if((parsePosition==1)&&(arguParse[0][0]=='.')&&(arguParse[0][1]=='/')){//If The Command is ./, then open the file and read it as command line
		shellScriptFlag=1;
		fileOpenControl=1;
		parseCommand(1);
	}	
}

int executeCommand(char **commandArray){//Execute each command line
	pid_t pid;
	pid_t wpid;
	int waitReturn;
	int waitControl=1;
	int chdirI;
	int forkReturn;
	int status;
	struct rusage ru;
	clock_t startTime, endTime;
	double totalTime;
	if(strcmp(commandArray[0],"cd")==0){
		chdirI=chdir(commandArray[1]); 
		if(chdirI==-1){
			perror("Path Changing Error");                      	
		}                                                           	
		else{                                                       	
			fprintf(stderr,"Changed Directory to %s\n",commandArray[1]);	
		}
	}
	else if(strcmp(commandArray[0],"exit")==0){
		fprintf(stderr,"Exit Shell, Exit Code %d\n",exitCode);
		exit(exitCode);
	}
	else if((strncmp(commandArray[0],"exit(",5)==0)&&(strcmp(&commandArray[0][bufLastByte],")")==0)){
		char exitBuf[4];
		int exitBufCode;
		for(int k=0;k<(bufLastByte-5);k++){
			exitBuf[k]=commandArray[0][k+5];
		}
		fprintf(stderr,"Exit Shell, Exit Code %s\n",exitBuf);
		exitBufCode=atoi(exitBuf);
		exit(exitBufCode);
	}
	else{	startTime=clock();
		pid=fork();
		if(pid==0){//Child Process
			int fd;
			if(stdoutRedirFlagT){
				if((fd=open(redirArray[0],O_CREAT|O_WRONLY|O_TRUNC,0666))<0){
					fprintf(stderr,"Open File Error, Can't open %s: %s\n",redirArray[0],strerror(errno));
					exit(1);
				}
				if(dup2(fd,1)<0){
					fprintf(stderr,"Can't dup2 %s as stdout: %s\n",redirArray[0],strerror(errno));
					exit(1);
				}
				close(fd);
			}
			if(stdoutRedirFlagA){
				if((fd=open(redirArray[0],O_CREAT|O_WRONLY|O_APPEND,0666))<0){
					fprintf(stderr,"Open File Error, Can't open %s:%s\n",redirArray[0],strerror(errno));
					exit(1);
				}
				if(dup2(fd,1)<0){
					fprintf(stderr,"Can't dup2 %s as stdout: %s\n",redirArray[0],strerror(errno));
					exit(1);
				}
				close(fd);
			}
			if(stderrRedirFlagT){
				if((fd=open(redirArray[0],O_CREAT|O_WRONLY|O_TRUNC,0666))<0){
					fprintf(stderr,"Open File Error, Can't open %s: %s\n",redirArray[0],strerror(errno));
					exit(1);
				}
				if(dup2(fd,2)<0){
					fprintf(stderr,"Can't dup2 %s as stderr: %s\n",redirArray[0],strerror(errno));
					exit(1);
				}
				close(fd);
			}
			if(stderrRedirFlagA){
				printf("*********\n");
				printf("%s\n",redirArray[0]);
				if((fd=open(redirArray[0],O_CREAT|O_WRONLY|O_APPEND,0666))<0){
					fprintf(stderr,"Open File Error, Can't open %s:%s\n",redirArray[0],strerror(errno));
					exit(1);
				}
				if(dup2(fd,2)<0){
					fprintf(stderr,"Can't dup2 %s as stderr: %s\n",redirArray[0],strerror(errno));
					exit(1);
				}
				close(fd);
			}
			if(stdinRedirFlag){
				if((fd=open(redirArray[0],O_RDONLY))<0){
					fprintf(stderr,"Open File Error, Can't open %s:%s\n",redirArray[0],strerror(errno));
					exit(1);
				}
				if(dup2(fd,0)<0){
					fprintf(stderr,"Can't dup2 %s as stdin: %s\n",redirArray[0],strerror(errno));
					exit(1);
				}
				close(fd);
			}
			forkReturn=execvp(commandArray[0],commandArray);
			if(forkReturn==-1){
				perror("Execute Error");
				exit(1);
			}
		}
		else if(pid<0){
			perror("Fork Error");
		}
		else{		//Parent Process	
			while(waitControl){
				if((waitReturn=wait3(&status,0,&ru))==-1){
					perror("Wait Error");
					waitControl=0;
				}
				else{	exitCode=WEXITSTATUS(status);
					if(WIFEXITED(status)){
						endTime=clock();
						totalTime=((double)(endTime-startTime))/CLOCKS_PER_SEC;
						fprintf(stderr,"Terminated Process:%d\nNormal Terminated, Exit Code :%d\n",waitReturn,WEXITSTATUS(status));
						fprintf(stderr,"%f seconds of realtime,%ld.%.6ld seconds of usertime,%ld.%.6ld seconds of systime\n",totalTime,ru.ru_utime.tv_sec,ru.ru_utime.tv_usec,ru.ru_stime.tv_sec,ru.ru_stime.tv_usec);
						waitControl=0;
					}
					else if(WIFSIGNALED(status)){
						if(WCOREDUMP(status)){
							fprintf(stderr,"Terminated Process:%d\nCore Dump\n",waitReturn);
						}
						else{
							fprintf(stderr,"Terminated Process:%d\nTerminated By Signal:%d\n",waitReturn,WTERMSIG(status));
						}
						waitControl=0;
					}
				}
			}
		}
	}
	return 0;
}
		
void main(){
	while(1){
		parseCommand(shellScriptFlag);
		int errorReport=0;
		errorReport=executeCommand(arguParse);
		stdoutRedirFlagT=0;
		stdoutRedirFlagA=0;
		stderrRedirFlagT=0;
		stderrRedirFlagA=0;
		stdinRedirFlag=0;
	}
}
