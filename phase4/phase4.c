
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <usyscall.h>
#include <provided_prototypes.h>
#include <errno.h>
#include "driver.h"
#include <time.h>

static int ClockDriver(char *arg);
static int DiskDriver(char *arg);
void sys_vec_handler4(sysargs *sa);
void assert_kernel_mode(char *message);

extern int start4(char *arg);

static int _clock_running;            // signal that clock driver is running
static int _disk_running[DISK_UNITS]; // signal that device drivers are ready

static int _disk_ready[DISK_UNITS]; // signal that disk is open, not busy.

int _wait_sem;
int _wake_up_time;
int _current_track = 0; // the driver arm
disk_request_ptr _working_request;

static struct driver_proc Driver_Table[MAXPROC];
//driver_proc_ptr _proc;
list _sleep_queue; // sleeping process queue
list _disk_queues[DISK_UNITS]; // disk driver queues for each unit
int _num_tracks[DISK_UNITS]; // track the number of tracks for each unit

int debugflag4;
int DEBUG4;

// shell functions so that I don't mix up semp and semv
int sem_wait(int sem)
{
    return semp_real(sem);
}

// shell functions so that I don't mix up semp and semv
int sem_release(int sem)
{
    return semv_real(sem);
}

int start3(char *arg)
{
    char name[128];
    char buf[128];
    char termbuf[10];
    int i;
    int clockPID;
    int pid;
    int status;
    int diskpids[DISK_UNITS]; // track pids for each unit

    /* Check kernel mode here.*/
    assert_kernel_mode("checking...\n");

    /* Assignment for system call handlers */
    for (int i = 12; i < 16; ++i)
    {
        sys_vec[i] = sys_vec_handler4;
    }

    /* Initialize the phase 4 process table */
    for (int i = 0; i < MAXPROC; ++i)
    {
        // phase 4 initialize processes
        Driver_Table[i].status = EMPTY;
        Driver_Table[i].sem_wait = semcreate_real(0);
    }

    /*
     * Create clock device driver
     * I am assuming a semaphore here for coordination.  A mailbox can
     * be used instead -- your choice.
     */
    _clock_running = semcreate_real(0);

    // initialize _sleep_queue list 
    list_init(&_sleep_queue);

    // initialize _disk_queues for each disk unit
    for (int i = 0; i < DISK_UNITS; i++)
    {
        list_init(&_disk_queues[i]);
    }

    // create proccess for the clock driver
    clockPID = fork1("Clock driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2);
    if (clockPID < 0)
    {
        console("start3(): Can't create clock driver\n");
        halt(1);
    }

    /*Wait for the clock driver to start */
    sem_wait(_clock_running);

    /* Create the disk device drivers here. */
    for (i = 0; i < DISK_UNITS; i++)
    {
        sprintf(buf, "%d", i);
        sprintf(name, "DiskDriver%d", i);
        // disk semaphore to signal the disk unit is running
        _disk_running[i] = semcreate_real(0);
        // disk semaphore to signal the disk unit is ready
        _disk_ready[i] = semcreate_real(1); 
        diskpids[i] = fork1(name, DiskDriver, buf, USLOSS_MIN_STACK, 2);
        if (diskpids[i] < 0)
        {
            console("start3(): Can't create disk driver %d\n", i);
            halt(1);
        }
    }

    // synchronize with disk driver
    for (i = 0; i < DISK_UNITS; i++)
    {
        sem_wait(_disk_running[i]);
    }

    /* Create first user-level process and wait for it to finish. */
    pid = spawn_real("start4", start4, NULL, 8 * USLOSS_MIN_STACK, 3);
    pid = wait_real(&status);

    /* Zap the clock driver */
    zap(clockPID); // clock driver
    join(&status); /* for the Clock Driver */

    /* Zap the device drivers */
    for (i = 0; i < DISK_UNITS; i++)
    {
        zap(diskpids[i]);
        semfree_real(_disk_ready[i]); // free the driver's signalling semaphore
        join(&status);                /* for the Disk Driver */
    }
    return 0;
}

void sys_vec_handler4(sysargs *sa)
{
    int result;
    int status;

    char *p_function_name = "undefined";

    if (sa == NULL)
    {
        console("sysCall(): Invalid syscall %d, no arguments.\n", sa->number);
        console("sysCall(): process %d terminating\n", getpid());
        terminate_real(1);
    }

    // switch statement to handle each type of syscall
    switch (sa->number)
    {
    case SYS_SLEEP:
        p_function_name = "SYS_SLEEP";
        // arg4: -1 if illegal values are given as input; 0 otherwise.
        sa->arg4 = (void *)sleep_real((int)sa->arg1);
        break;
    case SYS_DISKSIZE:
        p_function_name = "SYS_DISKSIZE";
        // arg1: size of a sector, in bytes
        // arg2: number of sectors in a track
        // arg3: number of tracks in the disk
        // arg4: -1 if illegal values are given as input; 0 otherwise.
        result = disk_size_real((int)sa->arg1, &sa->arg1, &sa->arg2, &sa->arg3);
        sa->arg4 = (void *)result;
        break;
    case SYS_DISKWRITE:
        p_function_name = "SYS_DISKWRITE";
        // arg1: 0 if transfer was successful; the disk status register otherwise
        // arg4: -1 if illegal values are given as input; 0 otherwise
        result = disk_write_real((int)sa->arg5, (int)sa->arg3, (int)sa->arg4, (int)sa->arg2, sa->arg1); 
        // handle return value from disk_write_real to pass back to diskWrite
        if (result == 0)
        {
            sa->arg1 = result;
            sa->arg4 = result;
        }
        else if (result == -1)
        {
            sa->arg4 = result;
        }
        else
        {
            sa->arg1 = result;
            sa->arg4 = 0;
        }

        break;
    case SYS_DISKREAD:
        p_function_name = "SYS_DISKREAD";
        // arg1: 0 if transfer was successful; the disk status register otherwise
        // arg4: -1 if illegal values are given as input; 0 otherwise
        result = disk_read_real((int)sa->arg5, (int)sa->arg3, (int)sa->arg4, (int)sa->arg2, sa->arg1); 
        // handle return value from disk_write_real to pass back to diskWrite
        if (result == 0)
        {
            sa->arg1 = result;
            sa->arg4 = result;
        }
        else if (result == -1)
        {
            sa->arg4 = result;
        }
        else
        {
            sa->arg1 = result;
            sa->arg4 = 0;
        }
        break;
    default:
        console("Bad system call number!");
        halt(1);
        break;
    }

    // if zapped when in the sys handler, then terminate
    if (is_zapped())
    {
        console("%s - Process zapped while in system call.", p_function_name);
        terminate_real(0);
    }
} /* sys_vec_handler4 */

static int ClockDriver(char *arg)
{
    int result;
    int status;
    int cur_time;

    // get slot on phase 4 proc table
    int pid = getpid();
    driver_proc_ptr proc = &Driver_Table[pid % MAXPROC];

    /* Let the clock driver know we are running and enable interrupts. */
    sem_release(_clock_running);
    psr_set(psr_get() | PSR_CURRENT_INT);
    while (!is_zapped())
    {
        result = waitdevice(CLOCK_DEV, 0, &status);
        if (result != 0)
        {
            return 0;
        }
        /*
         * Compute the current time and wake up any processes
         * whose time has come.
         */
        cur_time = sys_clock(); // get the current time
        // loop through the processes on the _sleep_queue
        for (int i = 0; i < _sleep_queue.length; i++)
        {   
            // look at value
            proc = list_at(&_sleep_queue, i); 
            if (cur_time >= proc->wake_time)
            {
                // signal the process to run
                sem_release(proc->sem_wait);
            }
        }
    }
    return 0; 
}

static int DiskDriver(char *arg)
{
    int unit = atoi(arg);
    device_request dev_req;
    int status;
    int dev_register;
    int tracks = 0;

    if (DEBUG4 && debugflag4)
        console("DiskDriver(%d): started\n", unit);

    psr_set(psr_get() | PSR_CURRENT_INT);
    /* Get the number of tracks for this disk */
    dev_req.opr = DISK_TRACKS;
    dev_req.reg1 = &tracks;

    // pass the number of disk tracks to tracks
    status = device_output(DISK_DEV, unit, &dev_req);

    // check status on device_output for DISK_DEV
    if (status != DEV_OK)
    {
        console("DiskDriver %d: did not get DEV_OK on DISK_TRACKS call\n", unit);
        console("DiskDriver %d: is the file disk%d present???\n", unit, unit);
        halt(1);
    }

    waitdevice(DISK_DEV, unit, &dev_register);

    // save the number of tracks to global variable
    _num_tracks[unit] = tracks;

    // signal that device drivers initialization process is done
    sem_release(_disk_running[unit]);

    // POST initalization
    while (!is_zapped())
    {
        // This is ineffecient way to query the disk queue
        status = waitdevice(CLOCK_DEV, 0, &dev_register);

        // if there is something in the unit's disk queue...
        if (_disk_queues[unit].length > 0)
        {
            // signal that the disk is open and not busy
            sem_wait(_disk_ready[unit]);

            // save the request locally
            disk_request *req = list_pop_front(&_disk_queues[unit]);

            // save number of sectors to write locally
            int num_sectors = req->sectors; // number of sectors to write
            // save the first sector index to write to
            int curr_sector_idx = req->first;
            // save the track that we'll be writing to
            int curr_track_idx = req->track;

            for (int i = 0; i < num_sectors; ++i)
            {
                // check that the sector isn't exceeding the number of sectors in a track
                if (curr_sector_idx >= DISK_TRACK_SIZE)
                {
                    // move to next track
                    curr_track_idx++;
                    // move to the first sector
                    curr_sector_idx %= DISK_TRACK_SIZE;
                }

                // check that we're not exceeding the number of tracks in the unit
                if (curr_track_idx >= _num_tracks[unit])
                {
                    return -1;
                }

                // seek to the requested track
                dev_req.opr = DISK_SEEK;
                dev_req.reg1 = curr_track_idx; // go to this track

                status = device_output(DISK_DEV, unit, &dev_req);
                status = waitdevice(DISK_DEV, unit, &dev_register);

                // ready the sector
                dev_req.opr = req->request_type == WRITE ? DISK_WRITE : DISK_READ; // if the request type is write, then set opr to DISK_WRITE, else set it to DISK_READ
                dev_req.reg1 = curr_sector_idx; // sector to read from
                dev_req.reg2 = req->buffer + (i * 512); // get the right portion of buffer to start operation from

                status = device_output(DISK_DEV, unit, &dev_req);
                status = waitdevice(DISK_DEV, unit, &dev_register);

                // the device_output operation completes, so we move to the next sector
                curr_sector_idx++;

                // only represents the last status
                req->completed_status = status;
                req->completed_register = dev_register;
            }

            sem_release(_disk_ready[unit]);
            sem_release(req->completed_sem);
        }
        // this process is zapped
    }
    return 0;

} /* DiskDriver */

/* processes requested delays for a specified number of seconds */
extern int sleep_real(int seconds)
{
    // error checking
    if (seconds < 0)
    {
        return 1;
    }
    // get the current pid
    int pid = getpid();
    // find the process a place on the Driver_Table
    driver_proc_ptr proc = &Driver_Table[pid % MAXPROC];
    // calculate the process's wakeup time
    proc->wake_time = sys_clock() + (seconds * CLOCKS_PER_SEC);

    // add proc to sleep queue
    list_push_back(&_sleep_queue, proc);
    // block
    sem_wait(proc->sem_wait);

    return 0;
} /* sleep_real */

extern int disk_read_real(int unit, int track, int first, int sectors, void *buffer)
{
    int proc_position;
    static int id;
    disk_request *current_request = malloc(sizeof(disk_request));

    // error check
    if (unit < 0 || unit >= DISK_UNITS)
    {
        return -1;
    }
    if (track < 0 || track >= _num_tracks[unit] || first >= DISK_TRACK_SIZE)
    {
        return -1;
    }

    // populate current_request
    current_request->request_type = READ;
    current_request->unit = unit;
    current_request->track = track;
    current_request->first = first; // first sector
    current_request->sectors = sectors;
    current_request->buffer = buffer;
    current_request->completed_sem = semcreate_real(0);


    // put request in request queue
    list_push_back(&_disk_queues[unit], current_request);

    // block so that device driver can complete request
    sem_wait(current_request->completed_sem);

    // select return values for sys_vec_handler4
    if (current_request->completed_status == 1)
        return 0;
    else
        return current_request->completed_status;
    return 0;
} /* disk_read_real */

// unit = disk ( 0 or 1 ), track = track index, first = first sector, sectors = number of sectors, buffer to write to
extern int disk_write_real(int unit, int track, int first, int sectors, void *buffer)
{
    int proc_position;
    static int id;
    disk_request *current_request = malloc(sizeof(disk_request));

    // error check
    if (unit < 0 || unit >= DISK_UNITS)
    {
        return -1;
    }
    if (track < 0 || track >= _num_tracks[unit] || first >= DISK_TRACK_SIZE)
    {
        return -1;
    }

    // populate current_request
    current_request->request_type = WRITE;
    current_request->unit = unit;
    current_request->track = track;
    current_request->first = first; // first sector
    current_request->sectors = sectors;
    current_request->buffer = buffer;
    current_request->completed_sem = semcreate_real(0);

    // put request in request queue
    list_push_back(&_disk_queues[unit], current_request);

    // block so that device driver can complete request
    sem_wait(current_request->completed_sem);

    if (current_request->completed_status == 1)
        return 0;
    else
        return current_request->completed_status;

} /* disk_write_real */

extern int disk_size_real(int unit, int *sector, int *track, int *disk)
{
    if (unit < 0 || unit > DISK_UNITS)
    {
        return -1;
    }

    *sector = DISK_SECTOR_SIZE; //always 512 according to USLOSS docs
    *track = DISK_TRACK_SIZE; // always 16 according to USLOSS docs
    *disk = _num_tracks[unit];

    return 0;
} /* disk_size_real*/

void assert_kernel_mode(char *message)
{
    int is_kernel_mode = (PSR_CURRENT_MODE & psr_get()) == 1 ? 1 : 0; // if PSR_CURRENT_MODE & psr_get() == 1, set is_kernel_mode = TRUE, else set_kernel_mode = FALSE

    if (is_kernel_mode == 0)
    {
        // not in kernel mode
        console("Kernel mode expected, but function called in user mode.\n");
        halt(1);
    }
} /* assert_kernel_mode */