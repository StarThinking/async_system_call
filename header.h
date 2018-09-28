#define ASYNC_TRUNC 0
#define ASYNC_OPEN 1

#define SUBMITTED 10
#define DONE 20

#define MMAP_PAGE_SIZE 4096

struct syscall_event {
    long pid;
    int syscall_type;
    long arg0;
    long arg1;
    int status;
    long ret;
};


