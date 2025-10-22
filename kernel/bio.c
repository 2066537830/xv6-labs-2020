// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13
#define HASH(id) (id%NBUCKET)

struct hashbuf {
  struct buf head;  // 每个桶的链表头
  struct spinlock lock; // 每个桶独立的锁
};

struct {
  struct buf buf[NBUF];
  struct hashbuf buckets[NBUCKET];  // 散列桶
} bcache;

void
binit(void)
{
  struct buf *b;

  char lockname[16];
  // 初始化每一个桶的锁
  for(int i=0; i<NBUCKET; i++){
    snprintf(lockname, sizeof(lockname), "bcache_%d", i);
    initlock(&bcache.buckets[i].lock, lockname);
    // 初始化每一个桶的头节点 head->prev、head->next都指向自身表示为空
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
    bcache.buckets[i].head.next = &bcache.buckets[i].head;
  }

  // 这段代码是将所有缓冲区插入到0号桶的双向链表中，采用的是头插法
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.buckets[0].head.next;  // 新节点的下一个节点等于头节点的下一个节点
    b->prev = &bcache.buckets[0].head;  // 新节点的前一个节点等于头节点
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].head.next->prev = b;  // 头结点的下一个节点的前一个节点等于新节点
    bcache.buckets[0].head.next = b;  // 头节点的下一个节点等于新节点
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  
  int bucketid = HASH(blockno);
  acquire(&bcache.buckets[bucketid].lock);

  // 遍历当前的桶中的缓存块，查找是否有已经缓存的块
  for(b = bcache.buckets[bucketid].head.next; b != &bcache.buckets[bucketid].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      // 记录使用时间
      acquire(&tickslock);
      b->timestamp = ticks;
      release(&tickslock);
      release(&bcache.buckets[bucketid].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  b = 0;
  struct buf* tmp;
  // Recycle the least recently used (LRU) unused buffer.
  // 从当前散列桶开始进行查找一个没有引用且timestamp最小的缓冲区，如果没找到就申请下一个桶
  for(int i=bucketid, cycle=0; cycle!=NBUCKET; i=(i+1)%NBUCKET, cycle++){
    // 如果遍历的是当前桶，则不需要获取锁（之前获取了没有释放），否则就需要获取锁
    if(i!=bucketid){
      if(!holding(&bcache.buckets[i].lock)){
        acquire(&bcache.buckets[i].lock);
      }
      else{
        continue;
      }
    }
    // 遍历桶中的每一个缓存块
    for(tmp = bcache.buckets[i].head.next; tmp != &bcache.buckets[i].head; tmp = tmp->next){
      // 使用时间戳实现LRU算法，查找一个没有引用且timestamp最小的缓冲区
      if(tmp->refcnt==0 && (b==0 || tmp->timestamp < b->timestamp)){
        b=tmp;
      }
    }
    // 找到了
    if(b){
      // 如果是从其他散列桶窃取的，则将其以头插法插入到当前桶
      if(i!=bucketid){
        // 将这个块从桶中删除
        b->prev->next = b->next;
        b->next->prev = b->prev;
        release(&bcache.buckets[i].lock);

        b->next = bcache.buckets[bucketid].head.next;
        b->prev = &bcache.buckets[bucketid].head;
        bcache.buckets[bucketid].head.next->prev = b;
        bcache.buckets[bucketid].head.next = b;
      }
      
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      acquire(&tickslock);
      b->timestamp = ticks;
      release(&tickslock);

      release(&bcache.buckets[bucketid].lock);
      acquiresleep(&b->lock);
      return b;
    }
    else{
      if(i!=bucketid){
        release(&bcache.buckets[i].lock);
      }
    }
  }
  release(&bcache.buckets[bucketid].lock);
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  int bucketid = HASH(b->blockno);
  releasesleep(&b->lock);

  acquire(&bcache.buckets[bucketid].lock);
  b->refcnt--;
  
  // 更新时间戳
  // 由于LRU改为使用时间戳判定，不再需要头插法
  acquire(&tickslock);
  b->timestamp = ticks;
  release(&tickslock);
  
  release(&bcache.buckets[bucketid].lock);
}

void
bpin(struct buf *b) {
  int bucketid = HASH(b->blockno);
  acquire(&bcache.buckets[bucketid].lock);
  b->refcnt++;
  release(&bcache.buckets[bucketid].lock);
}

void
bunpin(struct buf *b) {
  int bucketid = HASH(b->blockno);
  acquire(&bcache.buckets[bucketid].lock);
  b->refcnt--;
  release(&bcache.buckets[bucketid].lock);
}


