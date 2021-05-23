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
    int res_type = 0;
    int res_amnt = 0;

    int flags = 0;
    flags |= IPC_CREAT;
    
    struct sembuf sops[2];
    struct sembuf sops_wait[1];

    sops[0].sem_num = 0;
    sops[0].sem_op = 0;
    sops[0].sem_flg = 0;

    sops[1].sem_num = 0;
    sops[1].sem_op = 1;
    sops[1].sem_flg = 0;

    
    sops_wait[0].sem_num = 0;
    sops_wait[0].sem_op = -1;
    sops_wait[0].sem_flg = 0;
    

    int sem_id = semget(ftok(".",1), 1, flags | 0606);
    if (sem_id == -1) {
        perror("semid");
        exit(0);
    }

    printf("semid: %d\n", sem_id);

    union semun semval;
    semval.val = 0;

    if (semctl(sem_id, 0, SETVAL,semval) == -1) {
        perror("semctl");
        exit(0);
    }

    // Check for file size vs region size

    struct stat buffer;

    fstat(fileno(f), &buffer);

    printf("File size: %ld\n", buffer.st_size);

    char *mem_ptr;
    mem_ptr = static_cast<char*>(mmap((caddr_t)0, buffer.st_size, PROT_WRITE, MAP_SHARED, fileno(f), 0));

    if (mem_ptr == (caddr_t)-1) {
        printf("Error");
        fclose(f);
        return 0;
    }

    /*
    for(off_t i = 0; i < buffer.st_size; i++) {
        printf("found %c at %ji\n", mem_ptr[i], (intmax_t)i);
    }
    */

    for(int k = 0; k < 3; k++) {
        printf("%d : %c\n", k, (int)mem_ptr[resourceToIndex(k)]);
    }


    while (1) {
        printf("Specify resource type needed: ");
        scanf("%d", &res_type);
        printf("Specify resource amount needed: ");
        scanf("%d", &res_amnt);
        printf("Resource entered - Type: %d Amount: %d\n", res_type, res_amnt);

         if(semop(sem_id, sops, 2) == -1) {
            perror("semop");
            exit(EXIT_FAILURE);
        }

        if((res_amnt+48) > mem_ptr[resourceToIndex(res_type)]) {
            printf("Invalid - need more resources\n");
        } else {
            mem_ptr[resourceToIndex(res_type)] -= res_amnt;
        }

        for(int k = 0; k < 3; k++) {
            printf("%d : %c\n", k, (int)mem_ptr[resourceToIndex(k)]);
        }

        if (msync((void *)mem_ptr, buffer.st_size, MS_SYNC) == -1) {
            printf("msync error");
        }

        if(semop(sem_id, sops_wait, 1) == -1) {
            perror("semop wait");
            exit(EXIT_FAILURE);
        }

    }

    fclose(f);

    return 0;
}