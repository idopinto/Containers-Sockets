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
#define OPEN_ERR "open failed..."
#define EXECVP_ERR "execvp failed..."
#define REMOVE_ERR "remove failed..."
#define RM_DIR_ERR "rm dir failed..."
#define UMOUNT_ERR "umount failed..."

#define MALLOC_STACK_ERR "stack allocation failed"
#define MALLOC__ARGV_ERR "array allocation failed"

void err_n_die(const char *fmt, ...);
void delete_files_directories(void *args);

typedef struct ContainerInfo{

    char* new_host_name;
    char* new_filesystem_directory;
    char *num_processes;
    char* path_to_program_to_run_within_container;
    char** args_for_program;



}ContainerInfo;

//void write_to_file(char* filepath,char* to_write){
//    std::ofstream file;
//    file.open(filepath);
//    if(!file){
//        err_n_die(OPEN_ERR);
//    }
//    file << to_write;
//    file.close();
//}

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

    std::ofstream procs_file;
    procs_file.open("/sys/fs/cgroup/pids/cgroup.procs");
    if(!procs_file){
        err_n_die(OPEN_ERR);
    }
    procs_file << getpid();
    procs_file.close();

    std::ofstream max_file;
    max_file.open("/sys/fs/cgroup/pids/pids.max");
    if(!max_file){
        err_n_die(OPEN_ERR);
    }
    max_file << *ci->num_processes;
    max_file.close();

    std::ofstream on_release_file;
    on_release_file.open("/sys/fs/cgroup/pids/notify_on_release");
    if(!on_release_file){
        err_n_die(OPEN_ERR);
    }
    on_release_file << 1;
    on_release_file.close();


// mount the new procfs
    if(mount("proc","/proc","proc",0,0)<0){
        err_n_die(MOUNT_ERR);
    }
    if(execvp(ci->path_to_program_to_run_within_container,ci->args_for_program)< 0){
        err_n_die(EXECVP_ERR);
    }

    return 0;
}
void* create_new_process(void* args){
    void* stack = (void*)malloc(STACK);
    if (!stack){
        err_n_die(MALLOC_STACK_ERR);
    }

    // CLONE_NWEPID = new namespace of process IDs
    // SIGCHLD- send a signal to parent when child is done
    int child_pid = clone(child, (char*) stack + STACK,CLONE_NEWPID|CLONE_NEWUTS|CLONE_NEWNS|SIGCHLD,args);
    if (child_pid == -1){
        err_n_die(CLONE_ERR);
    }
    wait(NULL);
    return stack;
}

//int main(int argc, char* argv[]){
//    // parse from argv the program's argument we want to run. can be zero or more
//    char** arg_for_program= (char **) malloc(( argc - 3)*sizeof (char *));
//    if(!arg_for_program){
//        err_n_die(MALLOC__ARGV_ERR);
//    }
//    for(int i=0; i<argc -3; i++){
//        *(arg_for_program + i) =*(argv + i + 4);
//    }
//    *(arg_for_program + argc - 4) =nullptr;
//
//    // init
//    ContainerInfo containerInfo ={.new_host_name= argv[1],.new_filesystem_directory=argv[2],
//                                  .num_processes = argv[3],
//                                  .path_to_program_to_run_within_container=argv[4],
//                                  .args_for_program= arg_for_program};
//    // create new process
//    void* stack = create_new_process(&containerInfo);
//
//    // delete the files directories you created when you defind the cgorup for the container
//    delete_files_directories(&containerInfo);
//
//    // unmount the container's filesystem from the host
//    std::string filesystem=argv[2];
//    if (umount(const_cast<char *>((filesystem+"/proc").c_str())) < 0 ){
//        err_n_die(UMOUNT_ERR);
//    }
//
//    // free recourses
//    free(stack);
//    free(arg_for_program);
//    return 0;
//}

void remove_file(std::string filesystem ,const char* path_to_remove){
    filesystem += path_to_remove;
    if(remove(const_cast<char *>(filesystem.c_str())) < 0){
        err_n_die(REMOVE_ERR);
    }
}

void remove_directory(std::string filesystem ,const char* path_to_remove){
    filesystem += path_to_remove;
    if(rmdir(const_cast<char *>(filesystem.c_str())) < 0){
        err_n_die(RM_DIR_ERR);
    }
}

void delete_files_directories(void* args){
    ContainerInfo* ci = (ContainerInfo*) args;
    std::string fs = ci->new_filesystem_directory;
    remove_file(fs,"/sys/fs/cgroup/pids/notify_on_release");
    remove_file(fs,"/sys/fs/cgroup/pids/pids.max");
    remove_file(fs,"/sys/fs/cgroup/pids/cgroup.procs");
    remove_directory(fs,"/sys/fs/cgroup/pids");
    remove_directory(fs,"/sys/fs/cgroup");
    remove_directory(fs,"/sys/fs");
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