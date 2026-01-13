
#include "include/types.h"
#include "include/riscv.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/syscall.h"
#include "include/timer.h"
#include "include/kalloc.h"
#include "include/string.h"
#include "include/printf.h"
#include "include/sbi.h"

extern int exec(char *path, char **argv);

uint64
sys_exec(void)
{
  char path[FAT32_MAX_PATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;

  if(argstr(0, path, FAT32_MAX_PATH) < 0 || argaddr(1, &uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv)){
      goto bad;
    }
    if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
      goto bad;
    }
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    argv[i] = kalloc();
    if(argv[i] == 0)
      goto bad;
    if(fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }

  int ret = exec(path, argv);

  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;

 bad:
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}

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
  int mask;
  if(argint(0, &mask) < 0) {
    return -1;
  }
  myproc()->tmask = mask;
  return 0;
}

uint64
sys_shutdown(void) {
    sbi_shutdown();
    return 0;
}

#include "include/types.h"
#include "include/riscv.h"
#include "include/param.h"
#include "include/stat.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/sleeplock.h"
#include "include/file.h"
#include "include/pipe.h"
#include "include/fcntl.h"
#include "include/fat32.h"
#include "include/syscall.h"
#include "include/string.h"
#include "include/printf.h"
#include "include/vm.h"
//#include "include/proc.h"

/**
 * @brief 从系统调用参数（a0-a5寄存器）中获取一个用户空间的目标地址，然后将内核中的某块数据拷贝到这个目标地址去。
 * @param arg_index 系统调用参数的索引
 * @param dest 目标地址
 * @param size 数据大小
 * @return 0 成功，-1 失败
 */
 int get_and_copyout(uint64 arg_index, char* src, uint64 size) {
  uint64 dest_addr;
  if (argaddr(arg_index, &dest_addr) < 0) {
    return -1;
  }
  if (copyout2(dest_addr, src, size) < 0) {
    return -1;
  }
  return 0;
}

/**
 * @brief 实现 times 系统调用，返回自启动以来经过的操作系统 tick 数。
 * @param addr tms 结构体存到的目标地址
 * @return 0 成功，-1 失败
 */
 #include "include/timer.h"
 uint64 sys_times(void) {
  struct tms tms;

  acquire(&tickslock);
  tms.tms_utime = tms.tms_stime = tms.tms_cutime = tms.tms_cstime = ticks;
  release(&tickslock);

  if (get_and_copyout(0, (char *)&tms, sizeof(tms)) < 0) {
    return -1;
  }

  return 0;
}

/**
 * @brief 实现 uname 系统调用，返回操作系统名称和版本等信息。
 * @param addr 目标地址
 * @return 0 成功，-1 失败
 */
 uint64 sys_uname(void) {
  struct uname_info {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
  };

  // 这个数据当前是准备在内核的栈内存中的
  struct uname_info info = {
    "xv6",
    "xv6-node",
    "1.0.0",
    "1.0.0",
    "YanamiAnna",
    "localhost"
  };

  if (get_and_copyout(0, (char *)&info, sizeof(info)) < 0) {
    return -1;
  }

  return 0;
}

/**
 * @brief 实现 gettimeofday 系统调用，获取当前时间。
 * @param addr timespec 结构体存到的目标地址
 * @return 0 成功，-1 失败
 * @note 注意，根据测试样例的要求，需要返回 tv_usec 微秒而不是 Linux 标准中的 tv_nsec 纳秒
 */

 uint64 sys_gettimeofday(void) {
  struct timespec ts;
  uint64 htick = r_time(); // 硬件(hardware) tick，注意全局变量 ticks 是操作系统(os) tick，中间差了 200 倍

  ts.sec = htick / CLOCK_FREQ; // 换算成秒
  ts.usec = (htick % CLOCK_FREQ) * 1000000 / CLOCK_FREQ; // 换算成微秒, 1μs = 10^-6 s

  if (get_and_copyout(0, (char *)&ts, sizeof(ts)) < 0) {
    return -1;
  }
  return 0;
}

uint64 sys_nanosleep(void) {
  uint64 addr_tv;
  uint64 addr_rm;
  argaddr(0, &addr_tv);
  argaddr(1, &addr_rm);

  struct timespec interval;
  copyin2((char*)&interval, addr_tv, sizeof(struct timespec));

  int ticks_interval = interval.sec * TICKS_PER_SECOND + interval.usec * TICKS_PER_SECOND / 1000000;

  acquire(&tickslock);
  uint64 start = ticks;
  while(ticks < start + ticks_interval) {
    if(myproc() -> killed) {
      if(addr_rm != NULL) {
        uint64 rem_ticks = (ticks < start + ticks_interval) ? start + ticks_interval - ticks : 0;
        struct timespec rem_ts;
        rem_ts.sec = rem_ticks / TICKS_PER_SECOND;
        rem_ts.usec = (rem_ticks * 1000000) % TICKS_PER_SECOND;
        copyout2(addr_rm, (char*)&rem_ts, sizeof(struct timespec));
      }
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }

  release(&tickslock);
  return 0;
}

uint64 sys_clone(void) {
  return clone();
}

uint64
sys_wait(void)
{
  uint64 p;
  uint64 addr;

  argaddr(0, &p);
  if(argaddr(1, &addr) < 0) {
    return wait(-1, p);
  }
  return wait(p, addr);
}

uint64 sys_brk(void) {
  uint64 addr, new_addr;
  int delta;

  if (argaddr(0, &new_addr) < 0) {
    return -1;
  }

  addr = myproc()->sz;

  if (new_addr == 0) {
    return addr;
  }

  delta = new_addr - addr;

  if (growproc(delta) < 0) {
    return -1;
  }
  return 0;
}

uint64 sys_mmap(void) {
  uint64 addr, len;
  int prot, flags, fd, offset;

  argaddr(0, &addr);
  argaddr(1, &len);
  argint(2, &prot);
  argint(3, &flags);
  argint(4, &fd);
  argint(5, &offset);

  len = PGROUNDUP(len);

  struct proc *p = myproc();

  struct vma *v = NULL;
  for(int i = 0; i < NVMA; ++i) {
    if(p->vma[i].valid == 0) {
      v = &p->vma[i];
      break;
    }
  }

  if(v == NULL) {
    return -1;
  }

  struct file* f = NULL;
  if(!(flags & MAP_ANONYMOUS)) {
    f = p->ofile[fd];
  }

  v->start = mmap_getaddr(p, len);
  v->end = v->start + len;
  v->prot = prot;
  v->flags = flags;
  v->offset = offset;
  v->vm_file = (f == NULL) ? NULL : filedup(f);
  v->valid = 1;

  return v->start;
}

uint64 sys_munmap(void) {
  //printf("munmmap called\n");
  uint64 addr;
  int len;

  argaddr(0, &addr);
  argint(1, &len);

  len = PGROUNDDOWN(len);

  if(len == 0) return 0;

  struct proc *p = myproc();
  for(int i = 0; i < NVMA; ++i) {
    struct vma *v = &p->vma[i];
    if(v->valid && v->start == addr && (v->end - v->start) == len) {
      vma_writeback(p, v);
      uint64 npages = len / PGSIZE;
      int do_free = (v->flags & MAP_SHARED) == 0;
      vmunmap(p->pagetable, addr, npages, do_free);
      if(v->vm_file) {
        fileclose(v->vm_file);
        v->vm_file = NULL;
      }
      v->valid = 0;
      return 0;
    }
  }

  return -1;
}