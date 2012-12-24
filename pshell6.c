//## Pshell ##
//author :  Parag Gupta

#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<stdio.h>
#include<fcntl.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<signal.h>

#define STDOUT 1
#define STDIN 0
#define FF k=1;
#define alloc(t,w,x,y,z) { w = (t)realloc(w,x) ; w[y] = z; };
#define SIZE  256
#define chkerror(c,x) { if(c<0) perror(x); };

//global env variables for shell
char* HOME = NULL;
char* USER = NULL;
char* HOSTNAME = NULL;

void run();
void prompt(char*);
void pshell_header();
void putin_history(char*);
void execute_cmd(char*);
void signalhandler(int signum);
int check_user(int*,char***);
int i,j,k,bg;
void sig_child(int signum){
	int status;
	int pid = waitpid(-1,&status,WNOHANG);
	if(bg){
	if(pid > 0){
		printf("%d process exited normally\n",pid);
	}
	else{
		printf("process exited abnormally\n");
	}
	//printf("pid = %d status = %d\n",pid,status);
	fflush(stdout);
	}
	return ;
}

char **history = NULL;
int his_cmd = 0;
char dir[SIZE];
char ***argv=NULL; 
char ***redirected=NULL;
char cmd[SIZE];
int pipes,normal_args,mode; 
void run(){
	prompt(dir);
	fgets(cmd,SIZE,stdin);
	putin_history(cmd);
	execute_cmd(cmd);
}
int main(){
	signal(SIGINT,signalhandler); //Ctrl C 
	signal(SIGTERM,signalhandler); //kill
	signal(SIGCHLD,sig_child);

	system("clear");
	pshell_header();
	while(1) run();
	return 0;
}//shell ends

void execute_cmd(char* cmd){
	fflush(stdin);
	fflush(stdout);
	pipes = 0;
	bg = 0;
//	signal(SIGCHLD,SIG_DFL);
	alloc(char*** , argv , 4 , 0 , NULL); //initialize argv pointing to a NULL value ( HERE ARGV IS _NOT_ NULL BUT pointing to a NULL value, so argv[0]=NULL;
	alloc(char*** , redirected , 4 ,0, NULL);
	normal_args=0;
	char c;
	char* alpha = cmd;
	mode = 0; // normal mode;
	while((*alpha)!='\n'){
		c = *alpha;
		alpha++;
		switch(c){
			case '|':{
					 if(pipes!=0 || normal_args!=0){
						 pipes++;
						 normal_args = 0;
					 }
					 mode = 0; //normal mode
					 break;
				 }
			case '<':{ 
					 mode = 1;
					 break;
				 }
			case '>':{
					 mode = 2;
					 break;
				 }
			case '&':{
					 bg=1;
					// signal(SIGCHLD,sig_child);
					 break;
				 }
			case '\t':
			case ' ':{
					 if(mode == 0){ //if in normal mode increment normal_args otherwise dont do anything.
						 if(argv[pipes] && argv[pipes][normal_args])//here ignoring extra white spaces
							 normal_args++;
					 }
					 else {
						 if(redirected[mode]){
							 mode = 0 ; //again back to normal mode;
						 }
					 }
					 break;
				 }
			default :{
					 if(mode==0){//write in args
						 if(argv[pipes]==NULL){
							 alloc(char***,argv , pipes*4 + 8 , pipes+1 , NULL);
							 alloc(char**,argv[pipes] , 4 , 0 , NULL);
						 }

						 if(argv[pipes][normal_args]==NULL){
							 alloc(char**,argv[pipes] , normal_args*4 + 8, normal_args+1 , NULL); 
							 alloc(char*,argv[pipes][normal_args],1,0,'\0');
						 }
						 int pos = strlen(argv[pipes][normal_args]);
						 alloc(char*,argv[pipes][normal_args] , pos*1 + 2 , pos+1 ,'\0');
						 argv[pipes][normal_args][pos]=c;
					 }
					 else{ //write in redirected 
						 if(redirected[pipes]==NULL){
							 alloc(char*** , redirected , pipes*4 + 8 , pipes+1 , NULL); 
							 alloc(char**, redirected[pipes],16,0,NULL);
							 redirected[pipes][1]=NULL;
							 redirected[pipes][2]=NULL;
							 redirected[pipes][3]=NULL;
						 }
						 if(redirected[pipes][mode]==NULL){
							 alloc(char*,redirected[pipes][mode],1,0,'\0');
						 }
						 int pos = strlen(redirected[pipes][mode]);
						 alloc(char*,redirected[pipes][mode], pos*1 + 2 , pos+1 , '\0');
						 redirected[pipes][mode][pos] = c;
					 }
				 }
		}


	}//while reading char ends

	//processing cmd
	int pid=0;
	int pipes_used = pipes;
	//		fprintf(stderr,"pipes_used : %d\n",pipes_used);FF;
	//		fprintf(stderr,"%s %s \n",argv[pipes][0],argv[pipes][1]);FF;

	int p = 0;
	int pipefd[2] = {0};
	int in,out;
	int status;
	int d = dup(0); // previous descriptor....
	while(argv[p]){
		if(check_user(&p,argv)){
			p++;
		}
		else{
			if(argv[p+1]){ //if exists a pipe then make a pipe 
				pipe(pipefd);
			} 
			pid = fork();
			if(pid==0){ //child process;
				if(p>0){  
					//		fprintf(stderr,"reading from the previous pipe:%d \n",p);FF;
					close(0);
					dup(d);
					close(d);
				}
				if(argv[p+1]!=NULL){
					//		fprintf(stderr,"writing to pipefd instead of STDOUT\n");FF;
					close(1); //closing stdout
					dup(pipefd[1]); //creating alias for pipefd[1] which will be "1";
					close(pipefd[1]); //closing wont close the pipe as alias is there;
					close(pipefd[0]);  //closing read end of the pipe.
				}
				else{
					//	  perror("NULL ERRO");
				}
				if(redirected[p] && redirected[p][1]!=NULL){
					//		fprintf(stderr,"input redirection\n");
					in = open(redirected[p][1], O_RDONLY);
					close(0);
					dup(in);
					close(in);
				}
				if(redirected[p] && redirected[p][2]!=NULL){
					//		fprintf(stderr,"output redirection\n");
					out = open(redirected[p][2] ,O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
					close(1);
					dup(out);
					close(out);
				}
				if(execvp(argv[p][0],argv[p])==-1){
					perror("ERROR (command)");
					return ;
				}//all the child codes space is replaced by execvp command. :-)
			}
			//		wait(NULL);
			if(!argv[p+1] && bg==0){ 
				while(wait(NULL)!=pid);
				//	fprintf(stderr,"exiting..\n");FF;
			}
			if(argv[++p]){
				//backing up "read end" fd of pipe for use in next pipe for reading.
				d = dup(pipefd[0]); //aliasing read end of the pipe of the parent
				close(pipefd[1]); //closing write end of pipe
				close(pipefd[0]); //just removing pipefd[0] but d is already there.
			}
		}
	}
	//		printf("getting another cmd\n");fflush(stdout);

}
int check_user(int *r , char*** argv){
	int p = *r;
	if(strcmp(argv[p][0],"quit")==0){
		exit(0);
	}
	else if (strcmp(argv[p][0],"cd")==0){ 
		if(argv[p][1]){
			chkerror(chdir(argv[p][1]),"cd");
		}
		else chkerror(chdir(HOME),"cd");
		return 1;
	}
	else if(strcmp(argv[p][0],"hist")==0){
		int i;
		for(i=0;i<his_cmd;i++){
			printf("%s",history[i]);
		}
		return 1;
	}
	else if(strncmp(argv[p][0],"!hist",5)==0){
		char* tmp = &(argv[p][0][5]);
		int t = atoi(tmp);
		if(t==his_cmd-1){
			printf("can't execute\n");
			return 1;
		}
		if(t<his_cmd){
			execute_cmd(history[t]);
		}
		else{
			printf("can't execute\n");
		}
		return 1;
	}
	else if(strncmp(argv[p][0],"hist",4)==0){
		char* tmp = &(argv[p][0][4]);
		int t = atoi(tmp);
		if(his_cmd - t < 0){
			t = 0;
		}
		else t=his_cmd-t;
		for(;t!=his_cmd;t++){
			printf("%s",history[t]);
		}

		return 1;
	}
	else if(strcmp(argv[p][0],"pid")==0){
		if(argv[p][1]){
			if(strcmp(argv[p][1],"all")==0){

			}
			else if(strcmp(argv[p][1],"current")==0){
			}
			else perror("--usage : pid , pid all , pid current");
		}
		else{
			int tmp = getpid();
			printf("%d\n",tmp);
		}
		return 1;
	}
	return 0;
}

void prompt(char* dir){
	getcwd(dir,SIZE);
	char* tmp = dir;
	int flag=0;
	if(strstr(dir,HOME)!=NULL){
		int i;
		for(i=0;HOME[i]!='\0' && dir[i]==HOME[i];i++,tmp++);
		flag=1;
	}
	write(1,"<",1);
	write(1,USER,strlen(USER));
	write(1,"@",1);
	write(1,HOSTNAME,strlen(HOSTNAME));
	write(1,":",1);
	if(flag){
		write(1,"~",1);
		write(1,tmp,strlen(tmp));
	}
	else{
		write(1,tmp,strlen(tmp));
	}
	write(1,">",1);
}
void pshell_header(){
	char s[] = "Starting Pshell...\nCreated by:Parag Gupta\n";
	write(1,s,strlen(s));
	USER = (char*) malloc(sizeof(char)*SIZE);
	HOSTNAME = (char*) malloc(sizeof(char)*SIZE);
	HOME = (char*) malloc(sizeof(char)*SIZE);
	gethostname(HOSTNAME,SIZE);
	USER = getenv("USER");
	getcwd(HOME,SIZE);
	printf("USER : %s\n",USER);
	printf("HOSTNAME : %s\n",HOSTNAME);
	printf("HOME : %s\n",HOME);
}
void putin_history(char *cmd){
	if(strlen(cmd)!=1){
		alloc(char**,history,his_cmd*4+8,his_cmd+1,NULL);
		history[his_cmd] = (char*) malloc(sizeof(char)*(strlen(cmd)+2));
		strcpy(history[his_cmd],cmd);
		his_cmd++;
	}
}
void signalhandler(int signum){
}


/*
   cat a > b c d   =>  cat a c d > b 

 */
