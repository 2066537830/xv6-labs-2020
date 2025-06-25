// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

// 这是xv6操作系统的物理内存分配器实现，负责管理系统的物理内存资源
/*物理内存页面分配：为内核和用户进程分配4KB大小的内存页
内存释放：回收不再使用的内存页
初始化：启动时设置内存分配系统*/

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

// 空闲页链表节点
struct run {
  struct run *next;  // 指向下一个空闲页
};

// 全局内存管理结构
struct {
  struct spinlock lock;  // 保护并发访问
  struct run *freelist;  // 链表结构，每个节点代表一个空闲页
} kmem;

// 初始化函数
void
kinit()
{
  initlock(&kmem.lock, "kmem");
  // 内核以外的物理内存都被划分为页面并加入空闲链表，为后续内存分配做好准备
  freerange(end, (void*)PHYSTOP);   // end：内核代码的结束地址  PHYSTOP：物理内存的最高地址
}

// 将 pa_start 到 pa_end 这段物理内存划分为内存页（4096B）
void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);   // PGROUNDUP 是一个宏定义，用于将给定的大小 sz 向上对齐到页大小 PGSIZE 的倍数
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);   // 将每一个页面添加到空闲链表上
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
// 内存释放 将每个页面添加到空闲链表
void
kfree(void *pa)
{
  struct run *r;

  // 判定内存页是否合法 起始地址要是PGSIZE的倍数、要大于内核空间地址、要小于最大物理内存地址
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);  // 加锁
  r->next = kmem.freelist;  // 添加到头部
  kmem.freelist = r;
  release(&kmem.lock);  // 释放锁
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
// 内存分配 每次分配一个4096字节的空间
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;    // 取出空闲链表的头节点
  if(r)
    kmem.freelist = r->next;  // 原空闲链表的头节点成为新的头节点
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk  分配/释放时填充特殊值，帮助发现内存错误
  return (void*)r;
}

// 计算空闲的内存大小，只需要遍历freelist得到空白链表大小，再乘以PGSIZE就行啦
uint64
kcalc_freemem(void)
{
  struct run* r;
  int freepage=0;
  acquire(&kmem.lock);
  r = kmem.freelist;
  while(r){
    freepage++;   // 统计页表数
    r=r->next;
  }
  release(&kmem.lock);
  return freepage*PGSIZE;   // 返回的是字节数
}
