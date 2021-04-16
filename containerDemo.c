#define _GNU_SOURCE
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sched.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mount.h>
#include <error.h>
#include <sys/syscall.h>
#include <fcntl.h>

#define STACK_SIZE (1024 * 1024)    /* Stack size for cloned child */

/* A simple error-handling function: print an error message based
   on the value in 'errno' and terminate the calling process */

#define errExit(msg) {perror(msg); exit(EXIT_FAILURE);}

void mount_namespace(const char *file_system)
{
    if(mount(file_system,file_system,"ext4",MS_BIND,"") == -1)
        errExit("mount in mount_namespace");
    chdir(file_system);
    const char *old_fs = ".old_fs";

    if( rmdir(old_fs) == -1)
        errExit("rmdir old_fs");

    if( mkdir(old_fs,0777) == -1)
        errExit("mkdir old_fs");
        
    pivot_root(".",old_fs);
        // errExit("pivot root")
    chdir("/");
}

void pid_namespace()
{
    const char *mount_point = "/proc";
    if (mount("proc", mount_point, "proc", 0, NULL) == -1)
        errExit("mount in pid namespace");
    // printf("Mounting procfs at %s\n", mount_point);
}

void uts_namespace(const char *child_host_name)
{
    struct utsname uts;
    /* Change hostname in UTS namespace of child */
    if (sethostname(child_host_name, strlen(child_host_name)) == -1)
        errExit("sethostname");

    /* Retrieve and display hostname */
    if (uname(&uts) == -1)
        errExit("uname");
    printf("nodename in child uts namespace: %s\n", uts.nodename);
}

void network_namespace(const char *netspace_name)
{
    int fd = open(netspace_name,O_RDONLY | O_CLOEXEC);
    if(fd == -1)
        errExit("open");
    int ret_no = setns(fd,0);
    // printf("%d\n",temp);
    if(ret_no == -1)
        errExit("setns");
}

int namespace_handler(void *args)
{
    printf("NAMESPACE HANDLER\n");
    char **argv = (char **)args;

    const char *file_system = argv[1];
    // const char *file_system = "rootfs";
    mount_namespace(file_system);

    pid_namespace();

    const char *child_host_name = argv[2];
    uts_namespace(child_host_name);
    
    const char* nspace_name = argv[3];
    network_namespace(nspace_name);
    /* run bash shell in separete namespace */ 
    execlp("/bin/bash","/bin/bash",NULL);
    
    return 0;
}

int main(int argc, char *argv[])
{
    static char child_stack[STACK_SIZE];
    pid_t child_pid;
    struct utsname uts;
    printf("ENTER FILE_SYSTEM, HOST_NAME, NETWORK_NAMESPACE IN ORDER\n");
    if (argc < 3) 
    {
        fprintf(stderr, "Usage: %s <child-hostname>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Create a child that has its own mount,pid,uts namespace;
       the child commences execution in namespace_handler() */

    printf("CREATING NEW NAMESPACE\n");
    child_pid = clone(namespace_handler,
                        child_stack + STACK_SIZE,   /* Points to start of downwardly growing stack */ 
                        CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD,
                        argv);
    if (child_pid == -1)
    {
        errExit("clone");
    }
    // printf("PID of child created by clone() is %ld\n", (long) child_pid);

    /* Display the hostname in parent's UTS namespace. This will be 
       different from the hostname in child's UTS namespace. */

    if (uname(&uts) == -1)
        errExit("uname");    
    printf("nodename in parent uts namespace: %s\n", uts.nodename);

    if (waitpid(child_pid, NULL, 0) == -1)      /* Wait for child */
        errExit("waitpid");
    printf("END\n");
    exit(EXIT_SUCCESS);

    return 0;
}
