#ifndef _RX_KMUTEX_H_
#define _RX_KMUTEX_H_
#define RX_ENABLE_LOCKS         1
#define AFS_GLOBAL_RXLOCK_KERNEL

#define CV_INIT(cv,a,b,c)

#define CV_WAIT(cv, lck) {                                                \
                        }

#define CV_SIGNAL(cv)    {                                                \
                         }

#define CV_BROADCAST(cv) {                                                \
                         }

#define CV_DESTROY(a)

typedef sync_t  afs_kmutex_t;
typedef caddr_t afs_kcondvar_t;

#define osirx_AssertMine(addr, msg)

#define MUTEX_DESTROY(a)
#define MUTEX_ENTER(a)
#define MUTEX_TRYENTER(a) 1
#define MUTEX_EXIT(a)  
#define MUTEX_INIT(a,b,c,d) 

#endif
