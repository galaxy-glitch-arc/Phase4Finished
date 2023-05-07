#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "list.h"

#define MIN_CAPACITY 256
#define NOT_FOUND INT_MAX

void list_init(list *l)
{
  l->data = (long *)malloc(MIN_CAPACITY * sizeof(long));
  l->length = 0;
  l->capacity = MIN_CAPACITY;
}

void list_resize(list *l)
{
  if (l->length == l->capacity)
  {
    l->capacity *= 2;
    l->data = (long *)realloc(l->data, l->capacity * sizeof(long));
  }
}

void list_free(list *l)
{
  free(l->data);
  list_init(l);
}

void list_clear(list *l)
{
  l->length = 0;
}

void list_print(char* message, list *l)
{
  printf("%s:[ ", message);
  for (int i = 0; i < l->length; i++)
  {
    printf("%d ", l->data[i]);
  }
  printf(" ]\n");
} 

void list_push_front(list *l, long value)
{
  list_resize(l);

  for (int i = l->length; i > 0; i--)
  {
    l->data[i] = l->data[i - 1];
  }
  l->data[0] = value;
  l->length++;
}

long list_pop_front(list *l)
{
  if (l->length > 0)
  {
    long value = l->data[0];
    for (int i = 0; i < l->length - 1; i++)
    {
      l->data[i] = l->data[i + 1];
    }
    l->length--;
    return value;
  }

  return NOT_FOUND; // Error value indicating the vector is empty
}

void list_push_back(list *l, long value)
{
  list_resize(l);

  l->data[l->length] = value;
  l->length++;
}

void list_set_push(list *l, long value)
{
  if(list_exists(l, value) == 0)
  {
    list_push_back(l, value);
  }
}

long list_pop_back(list *l)
{
  if (l->length > 0)
  {
    long value = l->data[l->length - 1];
    l->length--;
    return value;
  }

  return NOT_FOUND; // Error value indicating the vector is empty
}

void list_delete(list *l, int index)
{
  if (index < 0 || index >= l->length)
  {
    return;
  }
  for (int i = index; i < l->length - 1; i++)
  {
    l->data[i] = l->data[i+1];
  }
  l->length--;
}


void list_remove(list *l, long value, int all)
{
  for (int i = 0; i < l->length; i++)
  {
    if (l->data[i] == value)
    {
      list_delete(l, i);
      if (!all)
      {
        return;
      }
    }
  }
}

int list_is_first(list *l, long value)
{
  if (l->data[0] == value)
  {
    return 1;
  }
  return 0;
}

long list_at(list *l, int index)
{
  if (index < 0 || index >= l->length)
  {
    return NOT_FOUND; // Error value indicating the index is out of bounds
  }

  return l->data[index];
}

//returns 1 if value is in vector, 0 if not
int list_exists(list *l, long value)
{
  for (int i = 0; i < l->length; i++)
  {
    if (l->data[i] == value)
    {
      return 1; // Value exists in the vector
    }
  }
  return 0; // Value does not exist in the vector
}

/*
int main()
{
  vector v;
  vector_init(&v);

  // Push elements onto the front and back of the vector
  vector_push_front(&v, 1);
  vector_push_front(&v, 2);
  vector_push_back(&v, 3);
  vector_push_back(&v, 4);

  // Print the vector
  printf("Vector: ");
  for (int i = 0; i < v.size; i++)
  {
    printf("%d ", v.data[i]);
  }
  printf("\n");

  // Pop elements from the front and back of the vector
  int popped_front = vector_pop_front(&v);
  int popped_back = vector_pop_back(&v);
  printf("Popped front: %d\n", popped_front);
  printf("Popped back: %d\n", popped_back);

  // Print the vector
  printf("Vector: ");
  for (int i = 0; i < v.size; i++)
  {
    printf("%d ", v.data[i]);
  }
  printf("\n");

  // Delete an element from the vector
  vector_delete(&v, 1);

  // Print the vector
  printf("Vector: ");
  for (int i = 0; i < v.size; i++)
  {
    printf("%d ", v.data[i]);
  }
}
*/
