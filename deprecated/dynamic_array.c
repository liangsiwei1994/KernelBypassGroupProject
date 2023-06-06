#include <middlebox.h>

void initArray(struct Array *a, size_t initialSize) {
  a->array = malloc(initialSize * sizeof(struct client_info));
  a->used = 0;
  a->size = initialSize;
}

void insertArray(struct Array *a, struct client_info element) {
  // a->used is the number of used entries, because a->array[a->used++] updates a->used only *after* the array has been accessed.
  // Therefore a->used can go up to a->size 
  if (a->used == a->size) {
    a->size *= 2;
    a->array = realloc(a->array, a->size * sizeof(struct client_info));
  }
  a->array[a->used++] = element;
}

void freeArray(struct Array *a) {
  free(a->array);
  a->array = NULL;
  a->used = a->size = 0;
}