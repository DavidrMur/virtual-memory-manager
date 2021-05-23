#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/ipc.h>
#include <sys/sem.h>

union semun {
    int val;
};

int resourceToIndex(int resource) {
    return resource*4+2;
}

int main() {
    FILE* f = fopen("res.txt", "a+");
    char *mem_ptr;
    int res_type = 0;
    int res_amnt = 0;
    int res_add = 0;
    int flags = 0;
    pid_t childpid;
    int parentToChildPipe[2];
    char *childReadBuffer;
    unsigned char *vec;
    void *addr;

    struct sembuf sops[2];
    struct sembuf sops_wait[1];
    struct stat buffer;


    int sem_id = semget(ftok(".",1), 1, flags | 0606);
    if (sem_id == -1) {
        perror("semid");
        //sys.exit();
    }

    // Check for file size vs region size
    fstat(fileno(f), &buffer);

    sops[0].sem_num = 0;
    sops[0].sem_op = 0;
    sops[0].sem_flg = 0;

    sops[1].sem_num = 0;
    sops[1].sem_op = 1;
    sops[1].sem_flg = 0;

    sops_wait[0].sem_num = 0;
    sops_wait[0].sem_op = -1;
    sops_wait[0].sem_flg = 0;
    
    flags |= IPC_CREAT;

    mem_ptr = static_cast<char*>(mmap(NULL, buffer.st_size, PROT_WRITE, MAP_SHARED, fileno(f), 0));

    vec = (unsigned char *)malloc((buffer.st_size+getpagesize()-1)/getpagesize());

    if((pipe(parentToChildPipe))== -1){
        perror("parentToChild");
        exit(0);
    }

    if((childpid = fork())== -1){
        perror("fork");
        exit(0);
    }

    if (childpid == 0) {

        while (1) {

            addr = mem_ptr + sizeof(mem_ptr)%getpagesize();
            
            printf("\nPage size: %d\n", getpagesize());
            
            // Critical region begins
            // Wait for semophore to become 0 then increment by 1 and start critical region
            if(semop(sem_id, sops, 2) == -1) {
                perror("semop");
                exit(EXIT_FAILURE);
            }
            for(int k = 0; k < 3; k++) {
                printf("Child - %d : %c\n", k, (int)mem_ptr[resourceToIndex(k)]);
            }
            // Decrement semophore by 1 at the end of the critical region
            if(semop(sem_id, sops_wait, 1) == -1) {
                    perror("semop wait");
                    exit(EXIT_FAILURE);
            }

            if(mincore(mem_ptr, buffer.st_size, vec) == 0){
                for (int j = 0; j < sizeof(vec)/sizeof(unsigned char*);j++) {
                    if(vec[j] == 0){
                        printf("page %d not in memory!\n", j);
                    }else{
                        printf("page %d in memory!\n", j);
                    }
                }
            }

            sleep(10);
        }

    } else {

        printf("semid: %d\n", sem_id);

        union semun semval;
        semval.val = 0;

        if (semctl(sem_id, 0, SETVAL,semval) == -1) {
            perror("semctl");
            //sys.exit();
        }

        printf("File size: %ld\n", buffer.st_size);
        
        if (mem_ptr == (caddr_t)-1) {
            printf("Error");
            fclose(f);
            return 0;
        }

        printf("mem_ptr parent: %d\n", mem_ptr);

        /*
        for(off_t i = 0; i < buffer.st_size; i++) {
            printf("found %c at %ji\n", mem_ptr[i], (intmax_t)i);
        }
        */

        for(int k = 0; k < 3; k++) {
            printf("%d : %c\n", k, (int)mem_ptr[resourceToIndex(k)]);
        }


        while (1) {
            printf("Add resources? (1-y/0-n): ");
            scanf("%d", &res_add);
            if (res_add) {
                printf("Specify resource type needed: ");
                scanf("%d", &res_type);
                printf("Specify resource amount needed: ");
                scanf("%d", &res_amnt);
                printf("Resource entered - Type: %d Amount: %d\n", res_type, res_amnt);

                // Critical Section begins

                if(semop(sem_id, sops, 2) == -1) {
                    perror("semop");
                    exit(EXIT_FAILURE);
                }

                if(res_amnt + mem_ptr[resourceToIndex(res_type)] > 57) {
                    printf("Invalid - too many resources\n");
                } else {
                    mem_ptr[resourceToIndex(res_type)] += res_amnt;
                }

                if (msync((void *)mem_ptr, buffer.st_size, MS_SYNC) == -1) {
                    printf("msync error");
                }

                for(int k = 0; k < 3; k++) {
                    printf("%d : %c\n", k, (int)mem_ptr[resourceToIndex(k)]);
                }

                
                if(semop(sem_id, sops_wait, 1) == -1) {
                    perror("semop wait");
                    exit(EXIT_FAILURE);
                }
                // Critical Section ends

            }

        }
    }

    fclose(f);

    return 0;
}