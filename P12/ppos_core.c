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
task_t *tp = NULL, *tc = NULL, *queue = NULL, dispatch, mainTask,semUp,semDown,semDest;
task_t *atual = NULL, *prox = NULL, *ts = NULL; //scheduler
task_t *atualw = NULL, *proxw = NULL;
int id = 1, mainSwitch = 0;
int taskType = 0;
long int totalTime;
task_t *queueSleep;


task_t *nextTask = NULL;


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

  task_create(&mainTask, (void *)ppos_init, "");
  task_create(&semUp, (void *)sem_up, "");
  task_create(&semDown, (void *)sem_down, "");
  task_create(&semDest, (void *)sem_destroy, "");
  tc = &mainTask;
};


// ------------ TASKS ------------
// Cria uma nova tarefa. Retorna um ID> 0 ou erro.
int task_create(task_t *task,               // descritor da nova tarefa
                void (*start_func)(void *), // funcao corpo da tarefa
                void *arg){

  // taskType = 0;

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
    task->execTime = 0;
    task->act = 0;
    task->id = id;
    task->queue = NULL;
    task->end = 0;
    task->wakeUp = -1;
    id++;

    task->context.uc_stack.ss_sp = stack;
    task->context.uc_stack.ss_size = STACKSIZE;
    task->context.uc_stack.ss_flags = 0;
    task->context.uc_link = 0;

    if (task == &semUp || task == &semDown || task == &semDest){
      id--;
      makecontext(&task->context, (void *)(*start_func), 1, (void *)arg);
      return 0;
    }

    if (task->id != 1) 
      queue_append((queue_t **) &queue, (queue_t *) task);
  }
  else
  {
    perror("Erro na criação da pilha: ");
    exit(1);
  }

  makecontext(&task->context, (void *)(*start_func), 1, (void *)arg);

  

  if (task == &mainTask)
  {
    id--;
    task->id = 0;
  }
  
  #ifdef DEBUG
    printf ("task_create: criou tarefa %d\n", task->id) ;
  #endif
  // taskType = 1;
  return (task->id);
};// argumentos para a tarefa

// alterna a execução para a tarefa indicada
int task_switch(task_t *task){
  taskType = 0;
  tp = tc;
  tc = task;
  tc->act++;
  if (mainSwitch == 0)
  {
    mainSwitch++;

    #ifdef DEBUG
      printf ("task_switch: trocando contexto 0 -> %d\n",task->id);
    #endif
    swapcontext(&mainTask.context, &task->context);
    mainSwitch=0;
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

  // taskType = 0;

  printf ("Task %d exit: execution time %ld ms, processor time  %ld ms, %ld activations\n", tc->id, totalTime, tc->execTime, tc->act);

  #ifdef DEBUG
    printf ("task_exit: tarefa %d sendo encerrada\n", tc->id) ;
  #endif

  if (queue_size((queue_t *) tc->queue) > 0)
  {
    task_t *aux = tc->queue;
    while (aux != NULL) {
      queue_remove((queue_t **) &tc->queue, (queue_t *) aux);
      queue_append((queue_t **) &queue,(queue_t *) aux);
      aux = tc->queue;
    }
  }
  
  

  if (queue_size((queue_t *) queue) > 0){
    queue_remove((queue_t **) &queue, (queue_t *) tc);
    tc->end = 1;
    tc->exitCode = exit_code;
    task_switch(&dispatch);
  }
  
  // if (tc == &mainTask)
    // exit(0);

  task_switch(&dispatch);


    

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
};

// Esta função devolve o valor da prioridade estática da tarefa task 
// (ou da tarefa corrente, se task for nulo).
int task_getprio (task_t *task) {
  if (task == NULL) 
    return tc->prioEstatica;
  else 
    return task->prioEstatica;
};


// a tarefa corrente aguarda o encerramento de outra task
int task_join (task_t *task) {
  
  if (!task || tc->end == 1)
    return -1;
  
  // task->queue = queue_append((queue_t **) &task->queue,(queue_t *) tc);
  task_t *aux = tc;
  queue_remove((queue_t **) &queue, (queue_t *) aux);
  queue_append((queue_t **) &task->queue,(queue_t *) tc);

  task_yield();
  return(task->exitCode);
};

// ------------ END - TASKS ------------

















// ------------ Sleeping ------------

// suspende a tarefa corrente por t milissegundos
void task_sleep (int t) {

  if (t == 0)
    task_yield();


  tc->wakeUp = totalTime + t;
  

  if (nextTask != NULL){
    if(nextTask->wakeUp > tc->wakeUp)
      nextTask = tc;
  }
  else
    nextTask = tc;

  task_t *aux = tc;
  queue_remove((queue_t **) &queue, (queue_t *) aux);
  queue_append((queue_t **) &queueSleep, (queue_t *) tc);

  task_yield();
};








void wakeUp(){
  task_t *aux;
  if (nextTask != NULL){
    aux = nextTask;

    queue_remove((queue_t **) &queueSleep, (queue_t *) nextTask);
    queue_append((queue_t **) &queue,(queue_t *) aux);
    // nextTask = NULL;

  }
  // if(queueSleep)
    if (queue_size((queue_t *) queueSleep) > 0)
    {
      // printf("\n\n\n %li \n\n\n",totalTime);      
      nextTask = queueSleep;
      proxw = queueSleep->next;

      while (proxw != queueSleep)
      {
        if (proxw->wakeUp <= nextTask->wakeUp)
          nextTask = proxw;

        proxw = proxw->next;
      }
    }
    else
      nextTask = NULL;
};




// ------------ END - Sleeping ------------























// ------------ Dispatcher & Escalonador ------------

// Política de escalonamento
task_t *scheduler()
{

  if(queue == NULL)
    return NULL;

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
  // taskType = 0;


  while (queue_size((queue_t *) queue) > 0 || nextTask != NULL){

    if(nextTask != NULL)
      if (nextTask->wakeUp <= totalTime){
        wakeUp();  

      }

    ts = scheduler();

    if(ts)
      task_switch(ts);


  }
  task_exit(0);
};

// ------------ END - Dispatcher & Escalonador ------------

















// ------------ TIMER ------------

void interruption() {

  totalTime++;
  tc->execTime++;

    if (tc->id != 1 && tc != &semUp && tc != &semDown && tc != &semDest)
    {

      tc->quantum = tc->quantum -1;
      tc->execTime++;
      if (tc->quantum == 0){
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
  totalTime = 0;
};



// retorna o relógio atual (em milisegundos)
unsigned int systime (){
  return(totalTime);
};

// ------------ END - TIMER ------------














// ------------ SEMAFOROS ------------


// cria um semáforo com valor inicial "value"
int sem_create (semaphore_t *s, int value) {

  if (s != NULL) {
    s->queue = NULL;
    s->counter = value;
    s->dead = 0;
    return (0);
  }

  return (-1);

};



// requisita o semáforo
int sem_down (semaphore_t *s) {


  if (!s->dead) {
    s->counter--;


    if (s->counter < 0) {
      task_t *aux = tc;
      queue_remove((queue_t **) &queue, (queue_t *) tc);
      queue_append((queue_t **) &s->queue, (queue_t *) aux);

      
      task_yield();
    }
    return (0);
  }
  

  return (-1);
};

// libera o semáforo
int sem_up (semaphore_t *s) {

  if (!s || s->dead)
    return -1;

  s->counter++;

  if (s->counter <= 0) {
    task_t *aux = s->queue; 
    queue_remove((queue_t **) &s->queue, (queue_t *) s->queue); 
    queue_append((queue_t **) &queue, (queue_t *) aux); 
  }

  return (0);
};

// destroi o semáforo, liberando as tarefas bloqueadas
int sem_destroy (semaphore_t *s) {

  if(!s || s->dead)
    return -1;

  task_t *aux;
  while (s->queue != NULL) {
    task_t *aux = s->queue;
    queue_remove((queue_t **) &s->queue, (queue_t *) s->queue);
    queue_append((queue_t **) &queue, (queue_t *) aux);
  }

  s->dead = 1;
  return (0);

};
// ------------ END - SEMAFOROS ------------











// ------------ Fila MSG ------------


// cria uma fila para até max mensagens de size bytes cada
int mqueue_create (mqueue_t *queue, int max, int size) {
  
  if (queue == NULL || max <= 0 || size <= 0) 
    return -1;

  queue->msgs = (void **) malloc(sizeof(void *)*max*size);
  

  queue->max = max;
  queue->size = size;

  queue->tam = 0;
  queue->prim = 0;
  queue->fim = 0;
  queue->dest = 0;

  sem_create(&queue->buffer, 1);
  sem_create(&queue->vaga, max);
  sem_create(&queue->item, 0);

  return 0;

};

// envia uma mensagem para a fila
int mqueue_send (mqueue_t *queue, void *msg) {

  if (queue && !queue->dest) {
        
        
    if (sem_down(&queue->vaga) == -1 || sem_down(&queue->buffer) == -1) 
      return -1;

    memcpy(&(queue->msgs[queue->fim]), msg, queue->size);
    queue->fim = (queue->fim + 1) % queue->max;
    queue->tam++;

    sem_up(&queue->buffer);
    sem_up(&queue->item);

    return (0);
  }

  return (-1);


};

// recebe uma mensagem da fila
int mqueue_recv (mqueue_t *queue, void *msg) {

 if (queue && !queue->dest) {
    if (sem_down(&queue->item) == -1 || sem_down(&queue->buffer) == -1)
      return -1;

    memcpy(msg, &(queue->msgs[queue->prim]), queue->size);
    queue->prim = (queue->prim + 1) % queue->max;
    queue->tam--;

    sem_up(&queue->buffer);
    sem_up(&queue->vaga);

    return (0);
  }

  return (-1);



};

// destroi a fila, liberando as tarefas bloqueadas
int mqueue_destroy (mqueue_t *queue) {

  if (queue && !queue->dest) {
    sem_down(&queue->buffer);

    free(queue->msgs);
    queue->dest = 1;

    sem_destroy(&queue->item);
    sem_destroy(&queue->vaga);
    sem_destroy(&queue->buffer);

    return (0);
  }

  return (-1);

};

// informa o número de mensagens atualmente na fila
int mqueue_msgs (mqueue_t *queue) {
  if (queue) return (queue->tam);

  return (-1); 
};