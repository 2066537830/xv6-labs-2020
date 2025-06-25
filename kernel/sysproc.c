// 与进程相关的系统调用处理函数 
// 采用相同的签名uint64 sys_xxx(void)

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_trace(void)
{
  // 允许用户程序通过 trace(mask) 来设置当前进程的追踪掩码
  int mask;
  if(argint(0, &mask)<0){   // argint() - 获取整数参数
    return -1;
  }
  myproc()->trace_mask=mask;
  return 0;
}

// 统计系统信息
uint64
sys_sysinfo(void)
{
  // 获取用户调用该系统调用的传入参数，由于传入的是一个地址，所以需要用一个unit64来接受
  uint64 uaddr;
  if(argaddr(0, &uaddr) < 0){   // 参数 0 代表从a0寄存器读取 传入参数保存到 a0-a5
    return -1;
  }
  struct sysinfo sysinfo;
  sysinfo.freemem = kcalc_freemem();
  sysinfo.nproc = active_procs();
  if(copyout(myproc()->pagetable, uaddr, (char*)&sysinfo, sizeof(sysinfo)) < 0){
    return -1;
  }
  return 0;
}
