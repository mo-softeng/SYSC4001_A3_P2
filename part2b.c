#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <time.h>
// Mohammed Osman 101312014
#define NUM_QUESTIONS 5
#define SHM_KEY 1234

struct SharedData {
    char rubric[NUM_QUESTIONS];  // holds A,B,C,D,E etc.
    int marked[NUM_QUESTIONS];   // 0 = unmarked, 1 = marked
    int current_exam;
    int stop;
};

sem_t *sem_rubric;
sem_t *sem_examloader;
sem_t *sem_question;

void random_delay(double min, double max) {
    double range = max - min;
    double r = ((double)rand() / RAND_MAX) * range + min;
    usleep((int)(r * 1000000)); 
}

void load_exam(struct SharedData *data, int exam_num) {
    char path[256];
    sprintf(path, "exams/%04d.txt", exam_num);

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("Could not open exam file %s\n", path);
        exit(1);
    }

    int student;
    fscanf(f, "%d", &student);
    fclose(f);

    data->current_exam = student;
    memset(data->marked, 0, sizeof(data->marked));

    printf("[Loader] Loaded exam for student %d\n", student);

    if (student == 9999)
        data->stop = 1;
}

void load_rubric(struct SharedData *data) {
    FILE *f = fopen("rubric.txt", "r");
    if (!f) {
        printf("Rubric file missing.\n");
        exit(1);
    }

    for (int i = 0; i < NUM_QUESTIONS; i++) {
        int qnum;
        char ch;
        fscanf(f, "%d, %c", &qnum, &ch);
        data->rubric[i] = ch;
    }
    fclose(f);
}

void save_rubric(struct SharedData *data) {
    FILE *f = fopen("rubric.txt", "w");
    for (int i = 0; i < NUM_QUESTIONS; i++)
        fprintf(f, "%d, %c\n", i + 1, data->rubric[i]);
    fclose(f);
}

void TA_process(int id, struct SharedData *data) {
    srand(getpid());

    while (!data->stop) {

        // ——— Rubric review ———
        sem_wait(sem_rubric);   // synchronize rubric modifications
        for (int i = 0; i < NUM_QUESTIONS; i++) {
            printf("[TA %d] Reviewing rubric Q%d\n", id, i+1);
            random_delay(0.5, 1.0);

            if (rand() % 5 == 0) { // 20% chance to modify
                data->rubric[i]++;
                printf("[TA %d] Updated rubric Q%d → %c\n",
                       id, i+1, data->rubric[i]);
                save_rubric(data);
            }
        }
        sem_post(sem_rubric);

        // ——— Marking questions ———
        while (1) {
            sem_wait(sem_question);
            int q = -1;

            for (int i = 0; i < NUM_QUESTIONS; i++) {
                if (!data->marked[i]) {
                    data->marked[i] = 1;
                    q = i;
                    break;
                }
            }

            sem_post(sem_question);

            if (q == -1) break; // no more questions

            printf("[TA %d] Marking Q%d for student %d\n",
                   id, q+1, data->current_exam);

            random_delay(1.0, 2.0);
        }

        // ——— Load next exam ———
        sem_wait(sem_examloader);
        if (!data->stop) {
            printf("[TA %d] Loading next exam…\n", id);
            load_exam(data, data->current_exam + 1);
        }
        sem_post(sem_examloader);
    }

    printf("[TA %d] Finished.\n", id);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: ./part2b <num_TAs>\n");
        return 1;
    }

    int n = atoi(argv[1]);
    if (n < 2) {
        printf("[ERROR] Must have >= 2 TAs.\n");
        return 1;
    }

    int shmid = shmget(SHM_KEY, sizeof(struct SharedData), 0666 | IPC_CREAT);
    struct SharedData *data = (struct SharedData*) shmat(shmid, NULL, 0);

    data->stop = 0;
    load_rubric(data);
    load_exam(data, 1);

    // create semaphores
    sem_rubric = sem_open("/rubric_sem", O_CREAT, 0666, 1);
    sem_examloader = sem_open("/exam_loader_sem", O_CREAT, 0666, 1);
    sem_question = sem_open("/question_sem", O_CREAT, 0666, 1);

    // Fork TAs
    for (int i = 0; i < n; i++) {
        if (fork() == 0) {
            TA_process(i + 1, data);
            exit(0);
        }
    }

    for (int i = 0; i < n; i++)
        wait(NULL);

    // cleanup
    sem_unlink("/rubric_sem");
    sem_unlink("/exam_loader_sem");
    sem_unlink("/question_sem");

    shmdt(data);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}
