#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <time.h>
// Mohammed Osman 101312014
#define NUM_QUESTIONS 5
#define MAX_LINE 64

struct SharedData {
    char rubric[NUM_QUESTIONS][MAX_LINE];
    int current_exam_student;
    int marked[NUM_QUESTIONS];
};

void random_delay(double min, double max) {
    double range = max - min;
    double r = ((double)rand() / RAND_MAX) * range + min;
    usleep((int)(r * 1000000)); // convert to microseconds
}

void load_rubric(struct SharedData *data) {
    FILE *f = fopen("rubric.txt", "r");
    if (!f) {
        perror("rubric.txt missing");
        exit(1);
    }

    for (int i = 0; i < NUM_QUESTIONS; i++) {
        fgets(data->rubric[i], MAX_LINE, f);
    }

    fclose(f);
}

void save_rubric(struct SharedData *data) {
    FILE *f = fopen("rubric.txt", "w");
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        fprintf(f, "%s", data->rubric[i]);
    }
    fclose(f);
}

void load_exam(struct SharedData *data, char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("Exam missing");
        exit(1);
    }

    fscanf(f, "%d", &data->current_exam_student);

    // reset marking state
    for (int i = 0; i < NUM_QUESTIONS; i++)
        data->marked[i] = 0;

    fclose(f);
}

void TA_process(struct SharedData *data, int id) {
    srand(getpid());

    while (1) {
        int student = data->current_exam_student;

        if (student == 9999) {
            printf("TA %d: Student 9999 reached. Stopping.\n", id);
            exit(0);
        }

        printf("TA %d: Reviewing rubric for student %d\n", id, student);

        // Check and maybe modify rubric
        for (int q = 0; q < NUM_QUESTIONS; q++) {
            random_delay(0.5, 1.0);

            if (rand() % 3 == 0) { // randomly decide change
                printf("TA %d: Changing rubric for question %d\n", id, q + 1);

                char *line = data->rubric[q];
                char *comma = strchr(line, ',');

                if (comma) {
                    comma[2] = comma[2] + 1; // bump letter
                }

                save_rubric(data);
            }
        }

        // Marking
        for (int q = 0; q < NUM_QUESTIONS; q++) {
            if (data->marked[q] == 0) {
                data->marked[q] = id; // marking by TA id
                printf("TA %d: Marking question %d for student %d\n",
                       id, q + 1, student);

                random_delay(1.0, 2.0);
            }
        }

        // One TA loads next exam
        if (id == 1) {
            static int examNum = 1;
            examNum++;

            char filename[64];
            sprintf(filename, "exams/%04d.txt", examNum);

            load_exam(data, filename);
            printf("TA 1: Loaded next exam %04d\n", examNum);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: ./a.out <num_TAs>\n");
        return 1;
    }

    int num_TAs = atoi(argv[1]);
    if (num_TAs < 2) {
        printf("Need at least 2 TAs.\n");
        return 1;
    }

    int shmid = shmget(IPC_PRIVATE, sizeof(struct SharedData), 0666 | IPC_CREAT);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }

    struct SharedData *data = (struct SharedData*) shmat(shmid, NULL, 0);

    load_rubric(data);
    load_exam(data, "exams/0001.txt");

    printf("Main: Loaded initial exam and rubric.\n");

    // Spawn TA processes
    for (int i = 1; i <= num_TAs; i++) {
        pid_t pid = fork();

        if (pid == 0) {
            TA_process(data, i);
            exit(0);
        }
    }

    while (wait(NULL) > 0) {}

    shmdt(data);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}
