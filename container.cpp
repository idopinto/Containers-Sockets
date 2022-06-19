//
// Created by idopinto12 on 06/06/2022.
//

#include "container.h"
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
#define SYS_ERR "system error:"
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
#define WAIT_ERR "wait failed..."

#define MALLOC_STACK_ERR "stack allocation failed"
#define MALLOC__ARGV_ERR "array allocation failed"



void error_massage(const char *fmt);
void delete_files_directories(void *args);

typedef struct ContainerInfo{

    char* new_host_name;
    char* new_filesystem_directory;
    char *num_processes;
    char* path_to_program_to_run_within_container;
    char** args_for_program;



}ContainerInfo;


/**
 * the new process function
 */
int child(void* args){
    ContainerInfo* ci = (ContainerInfo*) args;
    // change the hostname
    if(sethostname(ci->new_host_name,strlen(ci->new_host_name)) < 0){
        error_massage(CH_HOST_NAME_ERR);
    }
    // change root directory
    if(chroot(ci->new_filesystem_directory) < 0){
        error_massage(CHROOT_ERR);
    }
    // change the working directory into the new directory
    if(chdir(ci->new_filesystem_directory) < 0){
        error_massage(CHDIR_ERR);
    }


    // limit the number of processes that can run within the container

    if(mkdir("/sys/fs",DIR_MODE) < 0){
        error_massage(MKDIR_ERR);
    }
    if(mkdir("/sys/fs/cgroup",DIR_MODE) < 0){
        error_massage(MKDIR_ERR);
    }
    if(mkdir("/sys/fs/cgroup/pids",DIR_MODE) < 0){
        error_massage(MKDIR_ERR);
    }

    std::ofstream procs_file;
    procs_file.open("/sys/fs/cgroup/pids/cgroup.procs");
    if(!procs_file){
        error_massage(OPEN_ERR);
    }
    chmod("/sys/fs/cgroup/pids/cgroup.procs",DIR_MODE);
    procs_file << getpid();
    procs_file.close();

    std::ofstream max_file;
    max_file.open("/sys/fs/cgroup/pids/pids.max");
    if(!max_file){
        error_massage(OPEN_ERR);
    }
    chmod("/sys/fs/cgroup/pids/pids.max",DIR_MODE);

    max_file << *ci->num_processes;
    max_file.close();

    std::ofstream on_release_file;
    on_release_file.open("/sys/fs/cgroup/pids/notify_on_release");
    if(!on_release_file){
        error_massage(OPEN_ERR);
    }
    chmod("/sys/fs/cgroup/pids/notify_on_release",DIR_MODE);

    on_release_file << 1;
    on_release_file.close();


// mount the new procfs
    if(mount("proc","/proc","proc",0,0)<0){
        error_massage(MOUNT_ERR);
    }
    if(execvp(ci->path_to_program_to_run_within_container,ci->args_for_program)< 0){
        error_massage(EXECVP_ERR);
    }

    return 0;
}

/**
 * this function creates a new process with flags to separate its namespaces from the parents ,
 * and allocates a stack for the new process
 */
void* create_new_process(void* args){
    void* stack = (void*)malloc(STACK);
    if (!stack){
        error_massage(MALLOC_STACK_ERR);
    }

    // CLONE_NWEPID = new namespace of process IDs
    // SIGCHLD- send a signal to parent when child is done
    int child_pid = clone(child, (char*) stack + STACK,CLONE_NEWPID|CLONE_NEWUTS|CLONE_NEWNS|SIGCHLD,args);
    if (child_pid == -1){
        error_massage(CLONE_ERR);
    }
    if(wait(NULL)<0){ error_massage(WAIT_ERR);}
    return stack;
}


/**
 * this function deletes the files that we created
 */
void remove_file(std::string filesystem ,const char* path_to_remove){
    filesystem += path_to_remove;
    if(remove(const_cast<char *>(filesystem.c_str())) < 0){
        error_massage(REMOVE_ERR);
    }
}

/**
 * this function deletes the directories  that we created
 */
void remove_directory(std::string filesystem ,const char* path_to_remove){
    filesystem += path_to_remove;
    if(rmdir(const_cast<char *>(filesystem.c_str())) < 0){
        error_massage(RM_DIR_ERR);
    }
}
/**
 * this function deletes the files directories we created when we defined the cgroup for the container
 */
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
/**
 * this function prints the error massages and exit the program
 * @param fmt the massage
 */
void error_massage(const char *fmt){
    fprintf (stderr,"%s %s\n",SYS_ERR,fmt);
    exit (1);
}

int main(int argc, char* argv[]){
    // parse from argv the program's argument we want to run. can be zero or more
    char** arg_for_program= (char **) malloc(( argc - 3)*sizeof (char *));
    if(!arg_for_program){
        error_massage(MALLOC__ARGV_ERR);
    }
    *(arg_for_program)=*(argv + 4);
    for(int i=1; i<argc -4 ; ++i){
        *(arg_for_program + i ) =*(argv + i +4 );
    }
    *(arg_for_program + argc - 4) =nullptr;

    // init
    ContainerInfo containerInfo ={.new_host_name= argv[1],.new_filesystem_directory=argv[2],
                                  .num_processes = argv[3],
                                  .path_to_program_to_run_within_container=argv[4],
                                  .args_for_program= arg_for_program};
    // create new process
    void* stack = create_new_process(&containerInfo);

    // delete the files directories you created when you defind the cgorup for the container
    delete_files_directories(&containerInfo);

    // unmount the container's filesystem from the host
    std::string filesystem=argv[2];
    if (umount(const_cast<char *>((filesystem+"/proc").c_str())) < 0 ){
        error_massage(UMOUNT_ERR);
    }

    // free recourses
    free(stack);
    free(arg_for_program);
    return 0;
}