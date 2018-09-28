#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdlib.h>

#include "header.h"
 
static unsigned char * mmap_addr;
static int configfd;

static char * filename_string;

void async_trunc(const char * filename, int len) {
    struct syscall_event * event;

    printf("filename = %s, len = %d\n", filename, len);
    event = (struct syscall_event *) mmap_addr;
    event->syscall_type = ASYNC_TRUNC;
    memcpy(&(event->arg0), filename, strlen(filename)+1);
    event->arg1 = len;
    event->status = SUBMITTED; // pending
}

int async_open(const char * filename, unsigned short mode) {
    struct syscall_event * event;

    printf("filename = %s, mode = %u\n", filename, mode);
    event = (struct syscall_event *) mmap_addr;
    event->syscall_type = ASYNC_OPEN;
    memcpy(&(event->arg0), filename, strlen(filename)+1);
    event->arg1 = mode;
    event->status = SUBMITTED; // pending
    event->pid = getpid();

    while (event->status != DONE) ;
   
    printf("event->ret = %d\n", event->ret);
    return event->ret;
}


static void sig_handler(int signo) {
    printf("sig_handler.\n");
    close(configfd);
    free(filename_string);
    exit(0);
}

int main (int argc, char **argv) {
    int fd;
    int size;

    char buffer[20];

    filename_string = (char *) calloc(8, sizeof(char));

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGKILL, sig_handler);

    configfd = open("/sys/kernel/debug/mmap_example", O_RDWR);
    if(configfd < 0) {
        perror("Open call failed");
        return -1;
    }
     
    mmap_addr = mmap(NULL, MMAP_PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, configfd, 0);
    if (mmap_addr == MAP_FAILED) {
        perror("mmap operation failed");
        return -1;
    }

    fd = async_open("foo.txt", O_RDONLY);
    //async_trunc("/fo.txt", 2);

    if (fd < 0) {
        printf("Error fd < 0\n");
        goto error;
    } 

    printf("fd returned from async_open() is %d\n", fd);

    memset(buffer, 0, 20);
    size = read(fd, buffer, 10); 
    printf("read file content: %s\n", buffer);

    if (close(fd) < 0) { 
        perror("close"); 
        goto error;
    }

error:
    close(configfd);    
    free(filename_string);
    return 0;
}
