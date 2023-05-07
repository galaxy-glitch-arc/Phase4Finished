#include "list.h"

typedef struct driver_proc *driver_proc_ptr;
typedef struct disk_request *disk_request_ptr;

struct driver_proc
{
   driver_proc_ptr next_ptr;

   int wake_time; /* for sleep syscall */ // wake_up_time
   int been_zapped;

   /* Used for disk requests */
   int operation; /* DISK_READ, DISK_WRITE, DISK_SEEK, DISK_TRACKS */
   int track_start;
   int sector_start;
   int num_sectors;
   void *disk_buf;

   // more fields to add
   int sem_wait;
   int status;
};

typedef struct _disk_request
{
   int unit;
   int track;
   int first;
   int sectors;
   void *buffer;
   int request_type; // read or write
   int completed_sem;
   int completed_status; // 0 if false, 1 if true
   int completed_register;

} disk_request;

#define IN_USE 0
#define EMPTY 1
#define FREEING 2

#define READ 3
#define WRITE 4
