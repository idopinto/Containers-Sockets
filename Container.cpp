//
// Created by idopinto12 on 06/06/2022.
//

#include "Container.h"
#include <stdio.h>
#include <sched.h>
#include <cstdlib>
#include <csignal>
#include <sys/wait.h>
#include <sys/mount.h>

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fstream>
#include <iostream>

#define STACK 8192
#define DIR_MODE 0755
#define SYS_ERR "system error: (%d) %s\n"
#define CLONE_ERR "clone failed..."
#define CH_HOST_NAME_ERR "sethostname failed..."
#define CHROOT_ERR "chroot failed..."
#define CHDIR_ERR "chdir failed..."
#define MOUNT_ERR "mount failed..."
#define MKDIR_ERR "mkdir failed..."
#define OPEN_ERR "open failed"
#define EXECVP_ERR "execvp failed"
#define REMOVE_ERR "remove failed"
#define RM_DIR_ERR "rm dir failed"
#define UMOUNT_ERR "umount failed"
//#define ROOT_PATH "/cs/usr/idopinto12/OS/OS_EX5/ubuntu-base-14.04-core-arm64"

#define MALLOC_ERR "stack allocation failed"

void err_n_die(const char *fmt, ...);
void delete_files_directories();

typedef struct ContainerInfo{

    char* new_host_name;
    char* new_filesystem_directory;
    int *num_processes;
    char* path_to_program_to_run_within_container;
    char* const* args_for_program;

}ContainerInfo;

int child(void* args){
    ContainerInfo* ci = (ContainerInfo*) args;
    // change the hostname

    if(sethostname(ci->new_host_name,strlen(ci->new_host_name)) < 0){
        err_n_die(CH_HOST_NAME_ERR);
    }
    // change root directory
    if(chroot(ci->new_filesystem_directory) < 0){
        err_n_die(CHROOT_ERR);
    }
    // change the working directory into the new directory
    if(chdir(ci->new_filesystem_directory) < 0){
        err_n_die(CHDIR_ERR);
    }
    // mount the new procfs
    if(mount("proc","/proc","proc",0,0)<0){
        err_n_die(MOUNT_ERR);
    }

    // limit the number of processes that can run within the container
    if(mkdir("/sys/fs",DIR_MODE) < 0){
        err_n_die(MKDIR_ERR);
    }
    if(mkdir("/sys/fs/cgroup",DIR_MODE) < 0){
        err_n_die(MKDIR_ERR);
    }
    if(mkdir("/sys/fs/cgroup/pids",DIR_MODE) < 0){
        err_n_die(MKDIR_ERR);
    }

    std::fstream fptr1;
    std::fstream fptr2;
    std::fstream fptr3;
    fptr1.open("cgroup.procs");
    if(!fptr1){
        err_n_die(OPEN_ERR);
    }
    fptr1 << getpid(); // TODO correct?!

    fptr2.open("pids.max");
    if(!fptr2){
        err_n_die(OPEN_ERR);
    }
    fptr2 << ci->num_processes;

    fptr3.open("notify_on_release");
    if(!fptr3){
        err_n_die(OPEN_ERR);
    }
    fptr3 << 1;
//    //run the terminal / new program
    if(execvp(ci->path_to_program_to_run_within_container,ci->args_for_program)) < 0){
        err_n_die(EXECVP_ERR);
    }
}
int create_new_process(void* args){
    void* stack = (void*)malloc(STACK);
    if (!stack){
        err_n_die(MALLOC_ERR);
    }

    // CLONE_NWEPID = new namespace of process IDs
    // SIGCHLD- send a signal to parent when child is done
    int child_pid = clone(child, (char*) stack + STACK,CLONE_NEWPID|CLONE_NEWUTS|CLONE_NEWNS|SIGCHLD,args);
    if (child_pid == -1){
        err_n_die(CLONE_ERR);
    }
    wait(NULL);

}

int main(int argc, char* argv[]){
    if(argc != 6){
        // error?
    }

    ContainerInfo containerInfo ={.new_host_name= argv[1],.new_filesystem_directory=argv[2],
                                  .num_processes = reinterpret_cast<int*>(argv[3]),
                                  .path_to_program_to_run_within_container=argv[4],
                                  .args_for_program= reinterpret_cast<char *const *>(argv[5])};
    create_new_process(&containerInfo);
    delete_files_directories();
    if (umount("proc") < 0 ){
        err_n_die(UMOUNT_ERR);
    }
    // unmount the container's filesystem from the host
    // delete the files directories you created when you defind the cgorup for the container
    return 0;
}

void delete_files_directories(){
    if(remove("notify_on_release") < 0){
        err_n_die(REMOVE_ERR);
    }
    if(remove("pids.max") < 0){
        err_n_die(REMOVE_ERR);
    }
    if(remove("cgroup.procs") < 0){
        err_n_die(REMOVE_ERR);
    }
    if(rmdir("/sys/fs/cgroup/pids") < 0){
        err_n_die(RM_DIR_ERR);
    }
    if(rmdir("/sys/fs/cgroup") <0){
        err_n_die(RM_DIR_ERR);
    }
    if(rmdir("/sys/fs") < 0){
        err_n_die(RM_DIR_ERR);
    }
}

void err_n_die(const char *fmt,...){
    int errno_save;
    va_list  ap;
    // any system or library call can set errno, so we need to save it now
    errno_save = errno;

    // print out the fmt+args to standard out
    va_start(ap,fmt);
    vfprintf (stderr,fmt,ap);
    fprintf (stderr,"\n");
    fflush (stderr);

    // print out error message if errno was set.
    if(errno_save != 0){
        fprintf (stderr,SYS_ERR,errno_save, strerror (errno_save));
        fprintf(stderr,"\n");
    }
    va_end(ap);

    exit (1);
}