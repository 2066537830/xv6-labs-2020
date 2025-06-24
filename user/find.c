#include "kernel/types.h"
#include "kernel/stat.h"    // 定义了stat结构体
#include "kernel/fs.h"      // 定义了dirent结构体
#include "user/user.h"

void find(char* path, char* filename){
    
    int fd;
    struct dirent de;   // dirent结构体表示目录中的每个文件，包含包含 inum（inode 号）和 name（文件名）   
    struct stat st;     // stat结构体中有文件的属性，包括设备号、ionode号、文件类型（普通文件、目录、设备文件）等

    // 打开文件
    fd=open(path, 0);    // 只读权限
    if(fd<0){
        fprintf(STDERR_FILENO, "find: cannot open %s\n", path);
        return;
    }
    if(fstat(fd, &st)<0){
        fprintf(STDERR_FILENO, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }
    // 判断是文件类型
    if(st.type!=T_DIR){
        close(fd);
        return;
    }
    // 遍历目录项
    while(read(fd, &de, sizeof(de))==sizeof(de)){
        if(de.inum==0){
            continue;
        }
        if(strcmp(de.name, ".")==0 || strcmp(de.name, "..")==0){
            continue;
        }
        char new_path[512];
        memset(new_path, 0, sizeof(new_path));
        strcpy(new_path, path);
        strcpy(new_path+strlen(new_path), "/");
        strcpy(new_path+strlen(new_path), de.name);
        if(strcmp(de.name, filename)==0){
            printf("%s\n", new_path);
        }
        find(new_path, filename);
    }
    close(fd);
}

int main(int argc, char* argv[])
{
    if(argc<3){
        fprintf(STDERR_FILENO, "find: too less args\n");
        exit(-1);
    }
    find(argv[1], argv[2]);     // 指定目录 查找文件
    exit(0);
}