// GRR20180112 Bruno Augusto Luvizott 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <pthread.h>
#include "ppos.h"
#define STACKSIZE 64*1024


ucontext_t ContextMain;
task_t *tp = NULL, *tc = NULL, *next = NULL, *queue = NULL,dispatch;
int i = 0, id = 0, MainTask = 0;


// Cria uma nova tarefa. Retorna um ID> 0 ou erro.
int task_create(task_t *task,               // descritor da nova tarefa
                void (*start_func)(void *), // funcao corpo da tarefa
                void *arg){

  char *stack;
  
  getcontext(&task->context);

  stack = malloc(STACKSIZE);
  if (stack)
  {
    task->next = NULL;
    task->prev = NULL;
    task->id = id;
    id++;

    task->context.uc_stack.ss_sp = stack;
    task->context.uc_stack.ss_size = STACKSIZE;
    task->context.uc_stack.ss_flags = 0;
    task->context.uc_link = 0;

    if (task->id != 0) 
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
  
  return (task->id);
};// argumentos para a tarefa


// alterna a execução para a tarefa indicada
int task_switch(task_t *task){

  tp = tc;
  tc = task;
  if (MainTask == 0)
  {
    MainTask++;

    #ifdef DEBUG
      printf ("task_switch: trocando contexto 0 -> %d\n", task->id) ;
    #endif

    swapcontext(&ContextMain, &task->context);
    MainTask=0;
  }
  else
  {
    #ifdef DEBUG
      printf ("task_switch: trocando contexto 0 -> %d\n", task->id) ;
    #endif

    swapcontext(&tp->context, &tc->context);
  }
  return (0);
};

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit(int exit_code){
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

task_t *scheduler()
{
  queue = queue->next;
  return (queue->prev);
};

void dispatcher()
{
  while (queue_size((queue_t *) queue) > 0)
  {
    next = scheduler();
    task_switch(next);
  }
  task_exit(0);
};

// libera o processador para a próxima tarefa, retornando à fila de tarefas
// prontas ("ready queue")
void task_yield()
{
  task_switch(&dispatch);
};

// Inicializa o sistema operacional; deve ser chamada no inicio do main()
void ppos_init()
{
  setvbuf(stdout, 0, _IONBF, 0);
  task_create(&dispatch, (void *)dispatcher, "");
};