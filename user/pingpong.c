#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char* argv[])
{
    int fd1[2]; // 父进程写，子进程读
    int fd2[2]; // 父进程读，子进程写
    // fd[0]作为读取端，fd[1]作为写入端
    if(-1==pipe(fd1) || -1==pipe(fd2)){
        printf("pipe error\n");
        exit(-1);
    }
    int pid=fork();  // 创建一个子进程
    // 父进程中，pid是返回的子进程的PID
    // 子进程中，Pid返回值为0
    // 失败时，返回值为-1
    if(pid<0){
        printf("fork error\n");
        exit(-1);
    }
    else if(pid>0){
        // 父进程向子进程发送一个字节
        int nums=1;
        write(fd1[1], &nums, sizeof(nums));
        close(fd1[1]);
        int n=read(fd2[0], &nums, sizeof(nums));
        if(n<0){
            printf("father read error\n");
            exit(-1);
        }
        printf("%d: received pong\n", getpid());
        wait(0);    // 让父进程等待其子进程终止
    }
    else{ 
        // 子进程向管道中读数据
        int nums;
        int n=read(fd1[0], &nums, sizeof(nums));
        
        if(n<0){
            printf("son read error\n");
            exit(-1);
        }
        // 打印信息
        printf("%d: received ping\n", getpid());
        // 向父进程写入一个字节
        write(fd2[1], &nums, sizeof(nums));
        close(fd2[1]);
        exit(0);
    }
    close(fd1[0]);
    close(fd2[0]);
    exit(0);
}