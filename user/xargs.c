#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char* argv[])
{

    // 从标准输入中读取命令
    char buf[100], *p;
    p=buf;
    memset(buf, 0, sizeof(buf));
    char ch;
    int i=0;
    while(read(STDIN_FILENO, &ch, 1)==1){
        if(ch!='\n' && ch!=' '){
            buf[i++]=ch;
        }
        else{
            buf[i++]=0;
        }
    }
    // 解析将要执行的命令参数
    char* args[MAXARG];  // MAXARG在kernel/param.h中声明为32
    args[0]=argv[1];
    
    int index=1;
    // printf("1------\n");
    for(int i=2; i<argc; i++){
        // printf("args[%d]: %s\n", index, argv[i]);
        args[index++]=argv[i];
        
    }
    p=buf;
    // printf("2------\n");
    for(int i=0; ; i++){
        if(buf[i]=='\0'){
            // printf("args[%d]: %s\n", index, p);
            args[index++]=p;
            p=buf+i+1;
            if(buf[i+1]=='\0'){
                break;
            }
        }
    }
    args[index]=0;

    // 创建子进程
    int pid=fork();
    // 子进程
    if(pid==0){ 
        // // 使用exec执行终端传入的命令
        // printf("命令为：%s\n", argv[1]);
        // printf("参数个数为：%d, 参数为：", index+1);
        // for(int i=0; i<index; i++){
        //     printf("%s ", args[i], i);
        // }
        printf("\n");
        exec(argv[1], args);    
        fprintf(STDERR_FILENO, "xargs: exec failed\n"); // 如果 exec() 成功，这里不会执行, 因为进程的地址空间被exec执行的后续程序替换
        exit(-1);
    }
    if(pid>0){
        wait(0);
    }
    exit(0);
}