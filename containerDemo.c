#define _GNU_SOURCE
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sched.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mount.h>
#include <error.h>

#define STACK_SIZE (1024 * 1024)    /* Stack size for cloned child */

/* A simple error-handling function: print an error message based
   on the value in 'errno' and terminate the calling process */

#define errExit(msg) {perror(msg); exit(EXIT_FAILURE);}

void mount_namespace(const char *file_system)
{
    mount(file_system,file_system,"ext4",MS_BIND,"");
    chdir(file_system);
    const char *old_fs = ".old_fs";
    mkdir(old_fs,0777);
    pivot_root(".",old_fs);
    chdir("/");
}

void pid_namespace()
{
    const char *mount_point = "/proc";
    if (mount_point != NULL) 
    {
        if (mount("proc", mount_point, "proc", 0, NULL) == -1)
            errExit("mount");
        printf("Mounting procfs at %s\n", mount_point);
    }
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

int namespace_handler(void *args)
{
    printf("NAMESPACE HANDLER\n");
    char **argv = (char **)args;

    const char *file_system = argv[1];
    mount_namespace(file_system);

    pid_namespace();
    // const char *child_host_name = argv[1];
    // uts_namespace(child_host_name);

    execlp("bin/bash","bin/bash",NULL);
    return 0;
}

int main(int argc, char *argv[])
{
    static char child_stack[STACK_SIZE];
    pid_t child_pid;
    struct utsname uts;

    if (argc < 2) {
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
    printf("PID of child created by clone() is %ld\n", (long) child_pid);

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
