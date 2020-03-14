#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

/* Function pointers to hw3 functions */
void* (*mm_malloc)(size_t);
void* (*mm_realloc)(void*, size_t);
void (*mm_free)(void*);
size_t (*mm_size)(void*);

void load_alloc_functions() {
    void *handle = dlopen("hw3lib.so", RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    char* error;
    mm_malloc = dlsym(handle, "mm_malloc");
    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    mm_realloc = dlsym(handle, "mm_realloc");
    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    mm_free = dlsym(handle, "mm_free");
    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    mm_size = dlsym(handle, "mm_size");
    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }
}

struct ll {
	int num;
	struct ll * next;
};

int main() {
    load_alloc_functions();

    /*int *data = (int*) mm_malloc(sizeof(int));
    assert(data != NULL);
    data[0] = 0x162;
    mm_free(data);*/

    void * head = mm_malloc(1000);
    mm_free(head);

    int * data0 = mm_malloc(sizeof(int));
    *data0 = 162;

    int * data1 = mm_malloc(sizeof(int));
    *data1 = 163;

    mm_free(data0);
    mm_free(data1);
	
    printf("%ld is size of data block\n", mm_size(data1));
    //printf("%ld is size of data block\n", mm_size(data1));
    //printf("%ld is size of head block \n", mm_size(head));

    

    printf("malloc test successful!\n");
    return 0;
}
