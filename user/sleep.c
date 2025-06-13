// 实验目的：
// 1. 学习如何传递给程序的命令行参数
// 2. 学习如何使用sleep系统调用

// int main(int argc, char *argv[]) 是C/C++程序的入口函数，其中两个参数用于处理命令行参数：
// int argc (argument count)
// 表示命令行参数的数量
// 至少为1，因为程序名本身被视为第一个参数
// 例如：运行 ./program arg1 arg2 时，argc值为3

// *char argv[] (argument vector)
// 指向命令行参数的字符串数组
// argv[0] 通常是程序名称本身
// argv[1] 到 argv[argc-1] 是用户提供的命令行参数
// argv[argc] 总是NULL，表示数组结束

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    // 命令行输入：sleep 10 
    if(argc<2){
        printf("error: you should set a time\n");
        exit(-1);
    }
    sleep(atoi(argv[1]));
    // printf("sleep %d\n", atoi(argv[1]));
    exit(0);
}