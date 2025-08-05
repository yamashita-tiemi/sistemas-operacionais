#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <sched.h>
#include <linux/sched.h>

// Estrutura para armazenar informações de processos
typedef struct {
    pid_t pid;
    char name[256];
    char state;
} ProcessInfo;

// Estrutura para armazenar informações de threads
typedef struct {
    pid_t tid;
    pid_t pid;
    char name[256];
    char state;
} ThreadInfo;

// Estrutura para passar argumentos para thread
typedef struct {
    int thread_id;
    void* stack;
} ThreadArgs;

// Função que será executada pela thread
int thread_function(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    
    printf("Thread criada! TID: %ld, PID: %d\n", 
           syscall(SYS_gettid), getpid());
    
    // Simula trabalho da thread
    for (int i = 0; i < 3; i++) {
        printf("Thread %ld trabalhando... (%d/3)\n", syscall(SYS_gettid), i+1);
        sleep(1);
    }
    
    printf("Thread %ld terminando...\n", syscall(SYS_gettid));
    return 0;
}

// Função para criar processo usando syscall fork
pid_t create_process() {
    pid_t pid = syscall(SYS_fork);
    
    if (pid == -1) {
        perror("Erro ao criar processo (fork)");
        return -1;
    }
    
    if (pid == 0) {
        // Processo filho
        printf("Processo filho criado! PID: %d, PPID: %d\n", 
               getpid(), getppid());
        
        // Simula trabalho do processo filho
        for (int i = 0; i < 5; i++) {
            printf("Processo filho %d trabalhando... (%d/5)\n", getpid(), i+1);
            sleep(1);
        }
        
        printf("Processo filho %d terminando...\n", getpid());
        syscall(SYS_exit, 0);
    } else {
        // Processo pai
        printf("Processo pai criou filho com PID: %d\n", pid);
        return pid;
    }
}

// Função para criar thread usando syscall clone
pid_t create_thread() {
    // Aloca stack para a thread
    void* stack = malloc(8192);
    if (!stack) {
        perror("Erro ao alocar stack para thread");
        return -1;
    }
    
    // Prepara argumentos para a thread
    ThreadArgs* args = malloc(sizeof(ThreadArgs));
    if (!args) {
        free(stack);
        perror("Erro ao alocar argumentos para thread");
        return -1;
    }
    args->thread_id = 1;
    args->stack = stack;
    
    // Chama syscall clone para criar thread
    pid_t tid = syscall(SYS_clone, 
                       CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | 
                       CLONE_THREAD | CLONE_SYSVSEM,
                       (char*)stack + 8192,  // Stack cresce para baixo
                       NULL,  // parent_tidptr
                       NULL,  // child_tidptr
                       NULL); // tls
    
    if (tid == -1) {
        perror("Erro ao criar thread (clone)");
        free(stack);
        free(args);
        return -1;
    }
    
    if (tid == 0) {
        // Código da thread
        thread_function(args);
        free(args);
        free(stack);
        syscall(SYS_exit, 0);
    } else {
        printf("Thread criada com TID: %d\n", tid);
        return tid;
    }
}

// Função para listar processos lendo /proc
void list_processes() {
    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) {
        perror("Erro ao abrir /proc");
        return;
    }
    
    struct dirent* entry;
    ProcessInfo processes[100];
    int count = 0;
    
    printf("\n=== LISTA DE PROCESSOS ===\n");
    printf("%-8s %-20s %-8s\n", "PID", "NOME", "ESTADO");
    printf("------------------------------------\n");
    
    while ((entry = readdir(proc_dir)) != NULL && count < 100) {
        // Verifica se é um diretório numérico (PID)
        if (strspn(entry->d_name, "0123456789") == strlen(entry->d_name)) {
            char path[256];
            snprintf(path, sizeof(path), "/proc/%s/stat", entry->d_name);
            
            int fd = open(path, O_RDONLY);
            if (fd != -1) {
                char buffer[1024];
                int bytes = read(fd, buffer, sizeof(buffer) - 1);
                if (bytes > 0) {
                    buffer[bytes] = '\0';
                    
                    // Parse do arquivo stat
                    char* token = strtok(buffer, " ");
                    if (token) {
                        processes[count].pid = atoi(token);
                        
                        // Nome do processo (entre parênteses)
                        token = strtok(NULL, " ");
                        if (token) {
                            strncpy(processes[count].name, token, sizeof(processes[count].name) - 1);
                            processes[count].name[sizeof(processes[count].name) - 1] = '\0';
                            
                            // Estado do processo
                            token = strtok(NULL, " ");
                            if (token) {
                                processes[count].state = token[0];
                                
                                printf("%-8d %-20s %-8c\n", 
                                       processes[count].pid,
                                       processes[count].name,
                                       processes[count].state);
                                count++;
                            }
                        }
                    }
                }
                close(fd);
            }
        }
    }
    
    closedir(proc_dir);
    printf("\nTotal de processos listados: %d\n", count);
}

// Função para listar threads de um processo específico
void list_threads(pid_t pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/task", pid);
    
    DIR* task_dir = opendir(path);
    if (!task_dir) {
        printf("Erro ao abrir diretório de threads para PID %d\n", pid);
        return;
    }
    
    struct dirent* entry;
    ThreadInfo threads[50];
    int count = 0;
    
    printf("\n=== THREADS DO PROCESSO %d ===\n", pid);
    printf("%-8s %-8s %-20s %-8s\n", "TID", "PID", "NOME", "ESTADO");
    printf("--------------------------------------------\n");
    
    while ((entry = readdir(task_dir)) != NULL && count < 50) {
        if (strspn(entry->d_name, "0123456789") == strlen(entry->d_name)) {
            char stat_path[256];
            snprintf(stat_path, sizeof(stat_path), "/proc/%d/task/%s/stat", pid, entry->d_name);
            
            int fd = open(stat_path, O_RDONLY);
            if (fd != -1) {
                char buffer[1024];
                int bytes = read(fd, buffer, sizeof(buffer) - 1);
                if (bytes > 0) {
                    buffer[bytes] = '\0';
                    
                    char* token = strtok(buffer, " ");
                    if (token) {
                        threads[count].tid = atoi(token);
                        threads[count].pid = pid;
                        
                        token = strtok(NULL, " ");
                        if (token) {
                            strncpy(threads[count].name, token, sizeof(threads[count].name) - 1);
                            threads[count].name[sizeof(threads[count].name) - 1] = '\0';
                            
                            token = strtok(NULL, " ");
                            if (token) {
                                threads[count].state = token[0];
                                
                                printf("%-8d %-8d %-20s %-8c\n",
                                       threads[count].tid,
                                       threads[count].pid,
                                       threads[count].name,
                                       threads[count].state);
                                count++;
                            }
                        }
                    }
                }
                close(fd);
            }
        }
    }
    
    closedir(task_dir);
    printf("\nTotal de threads listadas: %d\n", count);
}

// Função para terminar processo usando syscall kill
int terminate_process(pid_t pid) {
    printf("Tentando terminar processo %d...\n", pid);
    
    // Primeiro tenta SIGTERM (terminação gentil)
    int result = syscall(SYS_kill, pid, SIGTERM);
    if (result == -1) {
        if (errno == ESRCH) {
            printf("Processo %d não encontrado\n", pid);
        } else {
            perror("Erro ao enviar SIGTERM");
        }
        return -1;
    }
    
    printf("SIGTERM enviado para processo %d\n", pid);
    
    // Aguarda um pouco para o processo terminar
    sleep(2);
    
    // Verifica se o processo ainda existe
    result = syscall(SYS_kill, pid, 0);
    if (result == 0) {
        printf("Processo %d ainda está rodando, enviando SIGKILL...\n", pid);
        result = syscall(SYS_kill, pid, SIGKILL);
        if (result == -1) {
            perror("Erro ao enviar SIGKILL");
            return -1;
        }
        printf("SIGKILL enviado para processo %d\n", pid);
    } else {
        printf("Processo %d terminado com sucesso\n", pid);
    }
    
    return 0;
}

// Função para obter informações detalhadas de um processo
void get_process_info(pid_t pid) {
    char path[256];
    
    // Lê arquivo status
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        printf("Processo %d não encontrado\n", pid);
        return;
    }
    
    char buffer[4096];
    int bytes = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("\n=== INFORMAÇÕES DO PROCESSO %d ===\n", pid);
        printf("%s\n", buffer);
    }
    
    close(fd);
}

// Menu principal
void show_menu() {
    printf("\n=== GERENCIADOR DE PROCESSOS E THREADS ===\n");
    printf("1. Criar processo\n");
    printf("2. Criar thread\n");
    printf("3. Listar processos\n");
    printf("4. Listar threads de um processo\n");
    printf("5. Terminar processo\n");
    printf("6. Informações detalhadas de processo\n");
    printf("7. Sair\n");
    printf("Escolha uma opção: ");
}

int main() {
    int choice;
    pid_t pid, tid;
    
    printf("Sistema de Gerenciamento de Processos e Threads\n");
    printf("Usando chamadas de sistema diretas do Linux\n");
    printf("PID atual: %d\n", getpid());
    
    while (1) {
        show_menu();
        if (scanf("%d", &choice) != 1) {
            printf("Entrada inválida!\n");
            while (getchar() != '\n'); // Limpa buffer
            continue;
        }
        
        switch (choice) {
            case 1:
                printf("\nCriando processo...\n");
                pid = create_process();
                if (pid > 0) {
                    printf("Aguardando processo filho terminar...\n");
                    int status;
                    waitpid(pid, &status, 0);
                    printf("Processo filho %d terminou com status %d\n", pid, status);
                }
                break;
                
            case 2:
                printf("\nCriando thread...\n");
                tid = create_thread();
                if (tid > 0) {
                    printf("Thread criada com sucesso!\n");
                    // Aguarda a thread terminar
                    sleep(4);
                }
                break;
                
            case 3:
                list_processes();
                break;
                
            case 4:
                printf("Digite o PID do processo para listar threads: ");
                if (scanf("%d", &pid) == 1) {
                    list_threads(pid);
                } else {
                    printf("PID inválido!\n");
                }
                break;
                
            case 5:
                printf("Digite o PID do processo para terminar: ");
                if (scanf("%d", &pid) == 1) {
                    terminate_process(pid);
                } else {
                    printf("PID inválido!\n");
                }
                break;
                
            case 6:
                printf("Digite o PID do processo para obter informações: ");
                if (scanf("%d", &pid) == 1) {
                    get_process_info(pid);
                } else {
                    printf("PID inválido!\n");
                }
                break;
                
            case 7:
                printf("Saindo...\n");
                exit(0);
                
            default:
                printf("Opção inválida!\n");
                break;
        }
    }
    
    return 0;
}