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

void assign_ip_and_make_veth_up(const char *veth0_name,const char *veth0_ip);
void delete_veth(const char *veth_name);

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

void pid_namespace(const char* mount_point)
{
    // const char *mount_point = "/proc";
    if(mount_point != NULL)
    {
        mkdir(mount_point,0555);
        if (mount("proc", mount_point, "proc", 0, NULL) == -1)
            errExit("mount in pid namespace");
        // printf("Mounting procfs at %s\n", mount_point);
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
    // printf("hostname in child uts namespace: %s\n", uts.nodename);
}

void network_namespace(const char *netspace_name)
{

    // printf("path:%s\n",netspace_name);
    char netns_path[64] = "/var/run/netns/";
    strcat(netns_path,netspace_name);
    int fd = open(netns_path,O_RDONLY | O_CLOEXEC);
    if(fd == -1)
        errExit("open");
    int ret_no = setns(fd,0);
    // printf("%d\n",temp);
    if(ret_no == -1)
        errExit("setns");
    system("ip link set dev lo up");
}

int namespace_handler(void *args)
{
    // printf("NAMESPACE HANDLER\n");
    char **argv = (char **)args;

    const char *file_system = argv[1];
    // const char *file_system = "rootfs";
    mount_namespace(file_system);

    const char* mount_point = argv[8];
    pid_namespace(mount_point);

    const char *child_host_name = argv[2];
    uts_namespace(child_host_name);
    
    /* Attach child process in network namespace */
    const char* nspace_name = argv[3];
    network_namespace(nspace_name);
    
    /* assign ip for child veth */
    const char* veth_name = argv[5];
    const char* veth_ip = argv[7];
    assign_ip_and_make_veth_up(veth_name,veth_ip);

    /* spawn a  shell */
    execlp("/bin/bash","/bin/bash",NULL);

    /* delete child namespace veth */
    delete_veth(veth_name);
    
    return 0;
}

void create_veth_pair(const char *child_netns_name,const char* veth0_name,const char *veth1_name)
{
    char cmd_buff[1024];
    sprintf(cmd_buff,"ip link add %s type veth peer name %s",veth0_name,veth1_name);
    system(cmd_buff);
    sprintf(cmd_buff,"ip link set %s netns %s",veth1_name,child_netns_name);
    system(cmd_buff);
}

void assign_ip_and_make_veth_up(const char *veth_name,const char *veth_ip)
{
    char cmd_buff[1024];
    sprintf(cmd_buff,"ip addr add %s/24 dev %s",veth_ip,veth_name);
    system(cmd_buff);
    sprintf(cmd_buff,"ip link set dev %s up",veth_name);
    system(cmd_buff);
}

void delete_veth(const char *veth_name)
{
	char cmd_buff[62];
	sprintf(cmd_buff,"ip link delete %s",veth_name);
	system(cmd_buff);
}

int main(int argc, char *argv[])
{
    static char child_stack[STACK_SIZE];
    pid_t child_pid;
    struct utsname uts;

    /* argv: file_name,file_system_name,child_host_name,netns_name,veth0_name,veth1_name,veth0_ip,veth1_ip */

    // printf("ENTER FILE_SYSTEM, HOST_NAME, NETWORK_NAMESPACE IN ORDER\n");
    if (argc < 3) 
    {
        fprintf(stderr, "Usage: %s <child-hostname>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* network namespace is created beforehand */

    /* Setup veth for network namespace */
    create_veth_pair(argv[3],argv[4],argv[5]);

    /* assign ip for default veth */
    assign_ip_and_make_veth_up(argv[4],argv[6]);

    /* Create a child that has its own mount,pid,uts namespace;
       the child commences execution in namespace_handler() */

    // printf("CREATING MOUNT, PID, UTS NAMESPACE\n");
    child_pid = clone(namespace_handler,
                        child_stack + STACK_SIZE,   /* Points to start of downwardly growing stack */ 
                        CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNET | SIGCHLD,
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
    // printf("hostname in parent uts namespace: %s\n", uts.nodename);

  
    /* memory limit using cgroups */
    // if(mkdir("/sys/fs/cgroup/memory/memory_limit",0777) == -1)
    //     errExit("mkdir memory_limit");
    system("echo 150000000 > /sys/fs/cgroup/memory/memory_limit/memory.limit_in_bytes");
    system("echo 0 > /sys/fs/cgroup/memory/memory_limit/memory.swappiness");

    char group_task_cmd[256];
    sprintf(group_task_cmd,"echo %d > /sys/fs/cgroup/memory/memory_limit/tasks",child_pid);
    system(group_task_cmd);

    /* waiting for child */
    if (waitpid(child_pid, NULL, 0) == -1)      /* Wait for child */
        errExit("waitpid");

   /* delete parent namespace veth */ 
    delete_veth(argv[4]);

    printf("END\n");
    exit(EXIT_SUCCESS);

    return 0;
}


/* EXTRAS */


// void create_network_namespace(const char *netns_name)
// {
//     char cmd_buff[1024];
//     sprintf(cmd_buff,"ip netns add %s",netns_name);
//     // printf("%s\n",cmd_buff);
//     system(cmd_buff);
//     system("ip netns list");
// }
