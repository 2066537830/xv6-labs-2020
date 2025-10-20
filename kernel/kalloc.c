// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  // 初始化所有的锁
  char lockname[8];
  for(int i=0; i<NCPU; i++){
    snprintf(lockname, sizeof(lockname), "kmem_%d", i); // 参数：buf, buf大小, 格式化字符串模板, 可变参数
    initlock(&kmem[i].lock, lockname);
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off(); // 关闭中断
  int id=cpuid();
  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&kmem[id].lock);
  pop_off();  // 打开中断
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int id=cpuid();
  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  // 如果当前cpu有空闲链表，直接使用
  if(r)
    kmem[id].freelist = r->next;
  // 如果没有，则窃取其他cpu的
  else {
    int cup_id;
    // 遍历所有cpu的空闲列表
    for(cup_id=0; cup_id<NCPU; cup_id++){
      if(cup_id==id){
        continue;
      }
      acquire(&kmem[cup_id].lock);
      r = kmem[cup_id].freelist;
      if(r){
        kmem[cup_id].freelist = r->next;
        release(&kmem[cup_id].lock);
        break;
      }
      release(&kmem[cup_id].lock);
    }
  }
  release(&kmem[id].lock);
  pop_off();
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
