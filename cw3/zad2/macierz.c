#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/file.h>

struct matrix{
    int cols;
    int rows;
    int **matrix;
};

struct matrix multiply(struct matrix A, struct matrix B, int start, int end){
    int sum = 0;
    struct matrix result;
    result.rows = A.rows;
    result.cols = 0;
    result.matrix = 
        (int**)calloc(end - start,sizeof(int));
    for(int j = start; j < end; j++){
        result.matrix[result.cols] = 
                (int*)calloc(A.rows,sizeof(int));
        for(int i = 0; i < A.rows; i++){
            for(int k = 0; k < A.cols; k++)
                sum+= A.matrix[i][k]*B.matrix[k][j];
            result.matrix[result.cols][i] = sum;
            sum = 0;
        }
        result.cols++;
    }
    return result;
}

struct matrix read_matrix(char* file_name){
    FILE* file = fopen(file_name,"r");
    struct matrix new_matrix;
    new_matrix.cols = 0;
    new_matrix.rows = 0;
    char t;
    do{
        fscanf(file,"%*d");
        t= fgetc(file);
        new_matrix.cols++;
    }while (t != '\n' && t != EOF);
    fseek(file,0,0);
    do{
        t= fgetc(file);
        if(t == '\n' || t == EOF)
        new_matrix.rows++;
    }while (t != EOF);
    fseek(file,0,0);
    new_matrix.matrix = 
        (int**)calloc(new_matrix.rows,sizeof(int*));
    for(int i = 0; i < new_matrix.rows; i++){
        new_matrix.matrix[i] = 
            (int*)calloc(new_matrix.cols,sizeof(int)); 
        for(int j = 0; j< new_matrix.cols; j++){
            fscanf(file,"%d",&(new_matrix.matrix[i][j]));
        }
    }
    fclose(file);
    return new_matrix;
}

void write_matrix(char* file_name, struct matrix M){
    FILE* file = fopen(file_name,"w");
    for(int i = 0; i< M.rows;i++){
        for(int j = 0; j< M.cols;j++){
            fprintf(file,"%d\t",M.matrix[j][i]);
        }
        fprintf(file,"\n");
    }
    fclose(file);
}
/*
startuje timer;
dopóki nie przekroczę czasu:
otwieram plik lista do odczytu;
wczytuję nazwy a b i c;
sprawdzam czy ilość kolumn b jest mniejsza niż ilość procesów:
tak: sprawdzam czy mój numer jest większy od ilości kolumn b
    tak: wczytuję kolejną linię z listy
    nie: zaczynam mnożyć kolumnę pod moim numerem
nie: zaczynam mnożyć fragment b od kolumny 
    (mój numer - ilość bloków które pominąłem)*ilość kolumn b / ilość wszystkich procesów 
    do (mój numer - ilość bloków które pominąłem + 1)*ilość kolumn b / ilość wszystkich procesów
*/


int proces(char* list, int workers_num, int max_time, int write_type){
    time_t start_time = time(NULL);
    int counter = 0;
    FILE* list_file = fopen(list,"r");
    FILE* msg = fopen("msg","r+");
    FILE* log = fopen("log","a+");
    int msg_d = fileno(msg);
    char *file_a = 
        (char*)calloc(FILENAME_MAX,sizeof(char)),
         *file_b = 
        (char*)calloc(FILENAME_MAX,sizeof(char)),
        *file_c = 
        (char*)calloc(FILENAME_MAX,sizeof(char)),
        *msg_line = 
        (char*)calloc(10,sizeof(char));
    int col_start, col_end, col_new_start, current_line, i=0; 
    struct matrix A,B,C;
    while((time(NULL) - start_time) < max_time){
        flock(msg_d,LOCK_EX);
        fgets(msg_line,10,msg);
        sscanf(msg_line,"%d\t%d",&current_line,&col_start);
        fseek(msg,0,0);
        if(current_line >= 0){
            fprintf(log,"before %d %d %d\n",getpid(), current_line,col_new_start);
            do{
                fscanf(list_file,"%s %s %s",file_a,file_b,file_c);
                i++;
            } while (i<current_line && getc(list_file) != EOF);
            i = current_line;
            A = read_matrix(file_a);
            B = read_matrix(file_b);
            if (B.cols < workers_num) col_end = col_start + 1;
            else col_end = col_start + B.cols/workers_num;
            col_new_start = col_end < B.cols ? col_end : 0;
            current_line = col_new_start == 0 ? i+1 : i;
            if(fgetc(list_file) == EOF 
            && col_new_start == 0) current_line = -1;
            fprintf(msg,"%d\t%d",current_line,col_new_start);
            fprintf(log,"after %d %d %d\n",getpid(), current_line,col_new_start);
            fseek(msg,0,0);
            flock(msg_d,LOCK_UN);
            C = multiply(A,B,col_start,col_end);
            write_matrix(file_c,C);
            counter++;
            free((void*)A.matrix);
            free((void*)B.matrix);
            free((void*)C.matrix);
        } else flock(msg_d,LOCK_UN);
    }
    free((void*)file_c);
    free((void*)file_b);
    free((void*)file_a);
    free((void*)msg_line);
    fclose(log);
    fclose(msg);
    fclose(list_file);
    return counter;
}

int run_processes(char* list,int max_workers,int max_time,int write_type){
    pid_t *children = 
        (pid_t*)calloc(max_workers,sizeof(pid_t));
    pid_t child_pid;
    for (int i =0; i< max_workers; i++){
        child_pid = fork();
        if(child_pid == 0){
            int result = proces(list,max_workers,max_time,write_type);
            exit(result);
        } else {
            children[i] = child_pid;
        }
    }
    int result;
    for (int i =0; i< max_workers; i++){
        waitpid(children[i], &result, 0);
        printf("Proces %d wykonał %d mnożeń\n", children[i],WEXITSTATUS(result));
    }
    remove("msg");
    return 0;
}

int main(int argc, char** argv){
    if (argc != 5) return 22;
    char* list = argv[1];
    int max_workers = atoi(argv[2]);
    int max_time = atoi(argv[3]);
    int write_type;
    if(strcmp(argv[4],"-c") == 0) write_type = 0;
    else if (strcmp(argv[4],"-s") == 0) write_type = 1;
    else return 22;
    FILE* msg = fopen("msg","w");
    fprintf(msg,"0\t0");
    fclose(msg);
    FILE* log = fopen("log","a+");
    fprintf(log,"%d %d\n",max_workers, max_time);
    fflush(log);
    return run_processes(list,max_workers,max_time,write_type);
}