#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 递归实现子进程筛选素数
void sieve(int pipefd[2]){
    int p;
    if(read(pipefd[0], &p, sizeof(p))==0){
        close(pipefd[0]);
        exit(0);
    }
    printf("prime %d\n", p);
    
    int cpipefd[2];
    pipe(cpipefd);
    int cpid;
    cpid=fork();
    if(cpid==0){
        close(cpipefd[1]);
        sieve(cpipefd);
    }
    else if(cpid>0){
        close(cpipefd[0]);
        int n;
        while(read(pipefd[0], &n, 4)){
            if(n%p!=0){
                write(cpipefd[1], &n, sizeof(n));
            }
        }
        close(cpipefd[1]);
        close(pipefd[0]);
        wait(0);
    }
    exit(0);
}

int main(int argc, char* argv[])
{
    int pipefd[2];
    pipe(pipefd);
    int pid;
    pid=fork();
    // 子进程
    if(pid==0){
        close(pipefd[1]);
        sieve(pipefd);
    }
    // 父进程
    else if(pid>0){
        close(pipefd[0]);
        for(int i=2; i<=35; i++){
            write(pipefd[1], &i, sizeof(i));
        }
        close(pipefd[1]);
        wait(0);
        exit(0);
    }
    exit(0);
}