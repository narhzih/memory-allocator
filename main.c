#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stddef.h>

typedef char ALIGN[16];
union header {
  struct {
    size_t size;
    unsigned is_free;
    struct header_t *next;
  } s;

  ALIGN stub;
};
typedef union header header_t;
header_t *head, *tail;
pthread_mutex_t global_malloc_lock;
header_t *get_free_block(size_t size);

void *malloc(size_t size)
{
  size_t total_size;
  header_t *header;
  void *block;

  if (!size) {
    return NULL;
  }
  pthread_mutex_lock(&global_malloc_lock);
  header = get_free_block(size);

  if(header) {
    header->s.is_free = 0;
    pthread_mutex_unlock(&global_malloc_lock);
    return (void *)(header + 1);
  }

  total_size = sizeof(header_t) + size;
  block = sbrk(total_size);
  if(block == (void *) -1) {
    pthread_mutex_unlock(&global_malloc_lock);
    return NULL;
  }
  header = block;
  header->s.size = size;
  header->s.is_free = 0;
  header->s.next = NULL;
  if (!head) {
    head = header;
  }

  if (tail) {
    tail->s.next = (struct header_t *) header;
  }
  tail = header;
  pthread_mutex_unlock(&global_malloc_lock);
  return (void *)(header + 1);
}

header_t *get_free_block(size_t size)
{
	header_t *curr = head;
	while(curr) {
		if (curr->s.is_free && curr->s.size >= size)
			return curr;
		curr = (header_t *) curr->s.next;
	}
	return NULL;
}

void free(void *block) {
  header_t *header, *tmp;
  void *programBreak;

  if (!block) {
    return;
  }

  pthread_mutex_lock(&global_malloc_lock);
  header = (header_t*) block - 1;
  programBreak = sbrk(0);

    if ((char*)block + header->s.size == programBreak) {
        if (head == tail) {
            head = tail = NULL;
        } else {
            tmp = head;
            while (tmp) {
                if((header_t *)tmp->s.next == (header_t *) tail) {
                    tmp->s.next = NULL;
                    tail = tmp;
                }
                tmp = (header_t *)tmp->s.next;
            }
        }
        sbrk(0 - sizeof(header_t) - header->s.size);
        pthread_mutex_unlock(&global_malloc_lock);
        return;
    }
    header->s.is_free = 1;
    pthread_mutex_unlock(&global_malloc_lock);
}
