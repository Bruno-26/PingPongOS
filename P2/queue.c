// Bruno Augusto Luvizott GRR20180112
#include <stdio.h>
#include "queue.h"


// Conta o numero de elementos na fila
// Retorno: numero de elementos na fila
int queue_size(queue_t *queue)
{
  queue_t *first = queue;
  queue_t *aux = queue;
  int tam = 1;
  
  if (!queue)
    return (0);

  while (aux->next != first)
  {
    aux = aux->next;
    tam++;
  }

  return (tam);
};



// Percorre a fila e imprime na tela seu conteúdo. A impressão de cada
// elemento é feita por uma função externa, definida pelo programa que
// usa a biblioteca. Essa função deve ter o seguinte protótipo:
//
// void print_elem (void *ptr) ; // ptr aponta para o elemento a imprimir
void queue_print (char *name, queue_t *queue, void print_elem (void*)){

  queue_t *first = queue;
  queue_t *aux = queue;

  if (!queue){
    printf("%s: []\n", name);
    return;  
  }

  printf("%s: [", name);
    
  while (aux->next != first){
    print_elem(aux);
    aux = aux->next;
    printf(" ");
  }
  print_elem(aux);
  printf("]\n");
};


//------------------------------------------------------------------------------
// Insere um elemento no final da fila.
// Condicoes a verificar, gerando msgs de erro:
// - a fila deve existir
// - o elemento deve existir
// - o elemento nao deve estar em outra fila
// Retorno: 0 se sucesso, <0 se ocorreu algum erro
int queue_append (queue_t **queue, queue_t *elem){

  queue_t *first;
  queue_t *aux;

  if (!queue)
    return -1;

  if (!elem)
    return -1;

  if (elem->next != NULL || elem->prev != NULL)
    return -1;

  if (!*queue)
  {
    first = elem;
    first->next = first;
    first->prev = first;
    *queue = (queue_t *)first;
    return 0;
  }
  else{
    first = *queue;
    aux = first;
    
    while (aux->next != first){
      aux = aux->next;
    }

    aux->next = elem;
    aux->next->next = first;
    aux->next->prev = aux;
    first->prev = aux->next;
    return 0;
  }
  
};

//------------------------------------------------------------------------------
// Remove o elemento indicado da fila, sem o destruir.
// Condicoes a verificar, gerando msgs de erro:
// - a fila deve existir
// - a fila nao deve estar vazia
// - o elemento deve existir
// - o elemento deve pertencer a fila indicada
// Retorno: 0 se sucesso, <0 se ocorreu algum erro
int queue_remove (queue_t **queue, queue_t *elem){
    
    queue_t *first = *queue;
    queue_t *aux = first;
    
    if (!queue) 
      return -1;

    if (queue_size(*queue) == 1 && first == elem){ 
        *queue = NULL;
        first->next = NULL;
        first->prev = NULL;
        return 0;
    }

    if (first == elem){
        *queue = first->next;
        first->next->prev = first->prev;
        first->prev->next = first->next;
        first->next = NULL;
        first->prev = NULL;
        return 0;
    }

    if (first->prev == elem)
    {
      aux = first->prev;
      aux->prev->next = aux->next;
      aux->next->prev = aux->prev;
      aux->next = NULL;
      aux->prev = NULL;
      return 0;
    }

    while (aux->next != first){
      if (aux == elem){
        aux->prev->next = aux->next;
        aux->next->prev = aux->prev;
        aux->next = NULL;
        aux->prev = NULL;
        return 0;
      }
      else 
        aux = aux->next;
    }
    
    return -1;
}