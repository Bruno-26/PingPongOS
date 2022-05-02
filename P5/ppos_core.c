// GRR20180112 Bruno Augusto Luvizott 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include "ppos.h"
#define STACKSIZE 64*1024


ucontext_t ContextMain;
task_t *tp = NULL, *tc = NULL, *queue = NULL,dispatch;
task_t *atual = NULL, *prox = NULL, *ts = NULL; //scheduler
int id = 1, MainTask = 0;
int taskType = 0;


// estrutura que define um tratador de sinal (deve ser global ou static)
struct sigaction action ;
// estrutura de inicialização to timer
struct itimerval timer ;


// Inicializa o sistema operacional; deve ser chamada no inicio do main()
void ppos_init()
{
  setvbuf(stdout, 0, _IONBF, 0);
  task_create(&dispatch, (void *)dispatcher, "");
  exec_timer();
};




// ------------ TASKS ------------
// Cria uma nova tarefa. Retorna um ID> 0 ou erro.
int task_create(task_t *task,               // descritor da nova tarefa
                void (*start_func)(void *), // funcao corpo da tarefa
                void *arg){
  taskType = 0;

  char *stack;
  
  getcontext(&task->context);

  stack = malloc(STACKSIZE);
  if (stack)
  {
    task->prioEstatica = 0;
    task->prioDinamica = 0;
    task->next = NULL;
    task->prev = NULL;
    task->quantum = 20;
    task->id = id;
    id++;

    task->context.uc_stack.ss_sp = stack;
    task->context.uc_stack.ss_size = STACKSIZE;
    task->context.uc_stack.ss_flags = 0;
    task->context.uc_link = 0;

    if (task->id != 1) 
      queue_append((queue_t **) &queue, (queue_t *) task);
  }
  else
  {
    perror("Erro na criação da pilha: ");
    exit(1);
  }

  
  #ifdef DEBUG
    printf ("task_create: criou tarefa %d\n", task->id) ;
  #endif

  makecontext(&task->context, (void *)(*start_func), 1, (void *)arg);
  
  // taskType = 1;
  return (task->id);
};// argumentos para a tarefa

// alterna a execução para a tarefa indicada
int task_switch(task_t *task){
  taskType = 0;
  tp = tc;
  tc = task;
  if (MainTask == 0)
  {
    MainTask++;

    #ifdef DEBUG
      printf ("task_switch: trocando contexto 0 -> %d\n",task->id);
    #endif
    taskType = 1;
    swapcontext(&ContextMain, &task->context);
    MainTask=0;
  }
  else
  {
    #ifdef DEBUG
      printf ("task_switch: trocando contexto %d -> %d\n",tp->id ,task->id);
    #endif

    taskType = 1;
    swapcontext(&tp->context, &tc->context);
  }
  return (0);
};

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit(int exit_code){
  taskType = 0;
  #ifdef DEBUG
    printf ("task_exit: tarefa %d sendo encerrada\n", tc->id) ;
  #endif

  queue_remove((queue_t **) &queue, (queue_t *) tc);

  if (queue_size((queue_t *) queue) > 0) 
    setcontext(&dispatch.context);
  
  setcontext(&ContextMain);
};

// retorna o identificador da tarefa corrente (main deve ser 0)
int task_id()
{
  return (tc->id);
};

// libera o processador para a próxima tarefa, retornando à fila de tarefas
// prontas ("ready queue")
void task_yield()
{
  task_switch(&dispatch);
};

// Esta função ajusta a prioridade estática da tarefa task para o valor prio 
// (que deve estar entre -20 e +20). Caso task seja nulo, ajusta a prioridade da tarefa atual.
void task_setprio (task_t *task, int prio) {
  if (task == NULL) {
    tc->prioEstatica = prio;
    tc->prioDinamica = prio;
  }
  else {
    task->prioEstatica = prio;
    task->prioDinamica = prio;
  }
}

// Esta função devolve o valor da prioridade estática da tarefa task 
// (ou da tarefa corrente, se task for nulo).
int task_getprio (task_t *task) {
  if (task == NULL) 
    return tc->prioEstatica;
  else 
    return task->prioEstatica;
}

// ------------ END - TASKS ------------


// ------------ Dispatcher & Escalonador ------------

// Política de escalonamento
task_t *scheduler()
{
  atual = queue;
  prox = queue->next;

  while (prox != queue) {
    if (prox->prioDinamica < atual->prioDinamica) {
      atual->prioDinamica = atual->prioDinamica - 1;
      atual = prox;
    }
    else
      prox->prioDinamica = prox->prioDinamica - 1;

    prox = prox->next;
  }

  atual->prioDinamica = atual->prioEstatica;
  
  
  return atual;
};


void dispatcher()
{
  while (queue_size((queue_t *) queue) > 0){
    taskType = 0;
    ts = scheduler();
    taskType = 1;
    task_switch(ts);
  }
    // task_switch(scheduler());

  task_exit(0);
};

// ------------ END - Dispatcher & Escalonador ------------

// ------------ TIMER ------------

// void interruption() {
//   if (taskType == 1)
//   {
//     if (tc->id > 1){
//       tc->quantum = tc->quantum -1;

//       if (tc->quantum == 0){
//         tc->quantum = 20;
//         task_yield();
//       }
//     }
//   }
// };

void interruption() {
  if (taskType == 1)
  {
    if (tc->id > 1 && tc->quantum > 0)
      tc->quantum = tc->quantum -1;
    else{
      tc->quantum = 20;
      task_yield();
    }
  }
};


void exec_timer() {

  action.sa_handler = interruption ;
  sigemptyset (&action.sa_mask) ;
  action.sa_flags = 0 ;

  if (sigaction (SIGALRM, &action, 0) < 0)
  {
    perror ("Erro em sigaction: ") ;
    exit (1) ;
  }

  // ajusta valores do temporizador
  timer.it_value.tv_usec = 1000 ;    // primeiro disparo, em micro-segundos
  timer.it_value.tv_sec  = 0 ;       // primeiro disparo, em segundos
  timer.it_interval.tv_usec = 1000 ; // disparos subsequentes, em micro-segundos
  timer.it_interval.tv_sec  = 0 ;    // disparos subsequentes, em segundos

  // arma o temporizador ITIMER_REAL (vide man setitimer)
  if (setitimer (ITIMER_REAL, &timer, 0) < 0)
  {
    perror ("Erro em setitimer: ") ;
    exit (1) ;
  }
};

// ------------ END - TIMER ------------

















