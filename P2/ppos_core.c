// Bruno Augusto Luvizott GRR20180112
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <pthread.h>
#include "ppos.h"
#define STACKSIZE 64*1024


task_t *tp = NULL, *tn = NULL, *tc = NULL;
ucontext_t ContextMain;
int i=0, id;


// Inicializa o sistema operacional; deve ser chamada no inicio do main()
void ppos_init(){
  setvbuf (stdout, 0, _IONBF, 0);
};

















// Cria uma nova tarefa. Retorna um ID> 0 ou erro.
int task_create(task_t *task,               // descritor da nova tarefa
                void (*start_func)(void *), // funcao corpo da tarefa
                void *arg){

  char *stack;
  getcontext(&task->context);

  stack = malloc(STACKSIZE);

  if (stack)
  {
    if (tp == NULL) {
      task->id = 1;
      tp = tn = task;
      task->next = tp;
      task->prev = tp;
    }
    else {
      task->id = tp->id + 1;
      task->next = tn;
      task->prev = tp;
      tp->next = task;
      tn->prev = task;
      tp = task;
    }
    task->context.uc_stack.ss_sp = stack ;
    task->context.uc_stack.ss_size = STACKSIZE ;
    task->context.uc_stack.ss_flags = 0 ;
    task->context.uc_link = 0 ;
  }
  else
  {
    perror("Erro na criação da pilha: ");
    exit(1);
  }

  makecontext (&task->context, (void*)(*start_func), 1, (void *) arg);
  
  #ifdef DEBUG
    printf ("task_create: criou tarefa %d\n", task->id) ;
  #endif
  
  return (task->id);
}; // argumentos para a tarefa


























// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit(int exitCode){
  #ifdef DEBUG
    printf ("task_exit: tarefa %d sendo encerrada\n",tc->id);
  #endif
  setcontext(&ContextMain);
};


























// alterna a execução para a tarefa indicada
int task_switch(task_t *task){

  // #ifdef DEBUG
  //   printf ("task_switch: trocando contexto %d -> %d\n",tc->id, task->id);
  // #endif
  tc = task;

  if (i == 0) {
    i++;
    #ifdef DEBUG
    printf ("task_switch: trocando contexto 0 -> %d\n", task->id);
    #endif
    swapcontext(&ContextMain, &task->context);
    i=0;
  }
  else {
    i++;
    #ifdef DEBUG
    printf ("task_switch: trocando contexto %d -> %d\n",task->prev->id, task->id);
    #endif
    swapcontext(&task->prev->context, &task->context);
  }

  return (0);
};
























// retorna o identificador da tarefa corrente (main deve ser 0)
int task_id(){
  return (tc->id);
};