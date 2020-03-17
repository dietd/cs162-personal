/*
 * mm_alloc.c
 */

#include "mm_alloc.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>


struct meta {
	size_t size;
	int free;
	struct meta * next;
	struct meta * prev;
	char contents[0];
};

struct meta * head;

/* Split's the block inputted by size of the new contents */
void split (struct meta * m, size_t size) {

  	 struct meta n;
	 n.size = m->size - size - sizeof(struct meta);
	 n.free = 1;
	 n.prev = m;
	 n.next = m->next;

	 void * naddr = (void *) (m->contents + size);

	 memcpy(naddr, &n, sizeof(struct meta));

	 m->next = naddr;
	 m->size = size;
	 m->free = 0;
}

void* mm_malloc(size_t size)
{
  if (size == 0)
	  return NULL;
  
  /* Base case if we are starting the heap */
  if (head == NULL) {
  	
	void * heap_start = sbrk(size + sizeof(struct meta));
	if (heap_start == -1)
		return NULL;

	struct meta m;
	m.size = size;
	m.free = 0;
	m.next = NULL;
	m.prev = NULL;
	
	memset(heap_start, 0, size + sizeof(struct meta));
	memcpy(heap_start, &m, sizeof(struct meta));
	
	head = heap_start;
	
	return head->contents;
  }
  
  /* Go through the list see if there is any available space,
   * if there is space and it's too much space split*/
  struct meta * m;
  struct meta * last = head;
  for (m = head; m->next != NULL; m = m->next) {

  	if (m->size >= size && m->free == 1) {
		
		memset(m->contents, 0, size);

		if (m->size - size >= sizeof(struct meta)) {
			split(m, size);
			return m->contents;
		}

		m->size = size;
		m->free = 0;

		return m->contents;
	}

	if (m->next == NULL)
		last = m;
  }

  if (m->size >= size && m->free == 1) {

                memset(m->contents, 0, size);

                if (m->size - size >= sizeof(struct meta)) {
                        split(m, size);
                        return m->contents;
                }

                m->size = size;
                m->free = 0;

                return m->contents;
       }

  void * addr = sbrk(size + sizeof(struct meta));
  if (addr == -1)
	  return NULL;

  struct meta n;
  n.size = size;
  n.free = 0;
  n.next = NULL;
  n.prev = last;

  memset(addr, 0, size + sizeof(struct meta));
  memcpy(addr, &n, sizeof(struct meta));

  last->next = addr;
  
  return ((struct meta *) addr)->contents;
}




void* mm_realloc(void* ptr, size_t size)
{

  /* Check the edge cases */
  if (size == 0) {
	 mm_free(ptr);
	 return NULL;
  }
  
  if (ptr == NULL && size <= 0)
	  return NULL;

  if (ptr == NULL && size > 0)
	  return mm_malloc(size);

  /*Free the pointer lmao*/
  struct meta * m = (struct meta *) (ptr - sizeof(struct meta));
  
  if (size < m->size){
	split(m, size);
  	return ptr;
  }


  mm_free(ptr);
  
  void * addr = mm_malloc(size);
  if (addr == NULL) {
	m->free = 0;
	return NULL;
  }
  
  memcpy(addr, m->contents, m->size);
  return addr;
}

/* Does unchecked merge of m0 <- m1 using m0's metadata struct */
void merge(struct meta * m0, struct meta * m1) {
        m0->next = m1->next;
        if (m1->next != NULL)
                m1->next->prev = m0;

        m0->size += sizeof(struct meta) + m1->size;
}

void mm_free(void* ptr)
{
  if (ptr == NULL)
  	return NULL;
  struct meta * m = (struct meta *) (ptr - sizeof(struct meta));
  
  m->free = 1;

  /*Store the metadata info*/
  struct meta * prev = m->prev;
  struct meta * next = m->next;

  /* Try merging adjacent blocks */
  if (prev != NULL && prev->free == 1) {
  	merge(prev, m);
	if (next != NULL && next->free == 1) {
		merge(prev, next);
	}
  } else {
  	if (next != NULL && next->free == 1) {
		merge(m, next);
	}
  }
}

size_t mm_size(void * ptr) {
	if (ptr == NULL)
		return -1 * sizeof(struct meta);
	return ((struct meta *) (ptr - sizeof(struct meta)))->size;
}

