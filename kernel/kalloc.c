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
} kmem;

struct {
  struct spinlock lock;
  int cnt[PHYSTOP / PGSIZE];
} ref;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ref.lock, "ref");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    ref.cnt[(uint64)p / PGSIZE] = 1;
    kfree(p);
  }
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

  // 每调用一次kfree，物理页的引用计数减一，只有当引用计数为0时，才释放物理内存，否则只做引用减一操作
  acquire(&ref.lock);
  if(--ref.cnt[(uint64)pa / PGSIZE] == 0){
    release(&ref.lock);
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  } else{
    release(&ref.lock);
  }
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
// 第一次分配物理内存时，设置引用计数为1
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    acquire(&ref.lock);
    ref.cnt[(uint64)r / PGSIZE] = 1;
    release(&ref.lock);
  }
  return (void*)r;
}

// 获取物理页面的引用计数
int
refcnt(void* pa)
{
  if((char*)pa < end || (uint64)pa >= PHYSTOP || (uint64)pa % PGSIZE != 0){
    return 0;
  }
  return ref.cnt[(uint64)pa / PGSIZE];
}

// 判断是不是cow页面
int
is_cow(pagetable_t pagetable, uint64 va)
{
  if(va >= MAXVA){
    return -1;
  }
  // 获取页表项
  pte_t* pte =  walk(pagetable, va, 0);
  if(pte == 0){
    return -1;
  }
  // 判断页表项是否有效
  if((*pte & PTE_V) == 0){
    return -1;
  }
  return (*pte & PTE_F ? 0 : -1);
}

// 增加内存的引用计数
int
addrefcnt(void* pa)
{
  if((char*)pa < end || (uint64)pa >= PHYSTOP || (uint64)pa % PGSIZE != 0){
    return -1;
  }
  acquire(&ref.lock);
  ref.cnt[(uint64)pa / PGSIZE]++;
  release(&ref.lock);
  return 0;
}

// cow分配器
void*
cowalloc(pagetable_t pagetable, uint64 va)
{
  // 参数检查，保证va是页面的起始地址
  if(va % PGSIZE != 0){
    return 0;
  }

  // 获取页表项
  pte_t* pte = walk(pagetable, va, 0);
  if(pte == 0){
    return 0;
  }
  // 获取物理地址
  uint64 pa = walkaddr(pagetable, va);
  // 获取物理页面的引用次数
  int n = refcnt((void*)pa);
  // 只剩一个进程对此物理页面有引用，则直接修改这个PTE
  if(n == 1){
    *pte |= PTE_W;
    *pte &= ~PTE_F;
    return (void*)pa;
  } else{
    // 有多个进程对这个物理页面有引用，复制到一个新的物理页
    uint64 mem = (uint64)kalloc();
    if(mem == 0){
      kfree((void*)mem);
      return 0;
    }
    // 复制新物理页
    memmove((void*)mem, (void*)pa, PGSIZE);
    // 将PTE映射到新的物理页
    *pte &= ~PTE_V; // 清除PTE_V，否则在mappagges中会判定为remap
    if(mappages(pagetable, va, PGSIZE, mem, (PTE_FLAGS(*pte) | PTE_W) & ~PTE_F) != 0){
      kfree((void*)mem);
      *pte |= PTE_V;
      return 0;
    }
    // 原始物理页面引用计数减一
    kfree((void*)PGROUNDDOWN(pa));
    return (void*)mem;
  }
}
