 /************************************
 * Written by Andreas Papadopoulos   *
 * http://akomaenablog.blogspot.com  *
 * akoma1blog@yahoo.com              *
 ************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pthread.h>
#include <sched.h>

#define MAX_THREAD 100
pthread_mutex_t         mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
	int start,end;
} param;

void *count(void *arg) {
	int i =0;
    int rc=0;

	param *p=(param *)arg;
	for(i =p->start ; i< p->end ; i++){
        rc = pthread_mutex_lock(&mutex);
		printf(" i = %d\n",i);sched_yield();
        rc = pthread_mutex_unlock(&mutex);

        rc=sched_yield();
	}
	return (void*)(1);
}

int main(int argc, char* argv[]) {
	int n,i;
	pthread_t *threads;
	param *p;

	if (argc != 2) {
		printf ("Usage: %s n\n",argv[0]);
		printf ("\twhere n is no. of threads\n");
		exit(1);
	}

	n=atoi(argv[1]);

	if ((n < 1) || (n > MAX_THREAD)) {
		printf ("arg[1] should be 1 - %d.\n",MAX_THREAD);
		exit(1);
	}

	threads=(pthread_t *)malloc(n*sizeof(*threads));

	p=(param *)malloc(sizeof(param)*n);
	/* Assign args to a struct and start thread */
	for (i=0; i<n; i++) {
		p[i].start=i*100;
		p[i].end=(i+1)*100;
		pthread_create(&threads[i],NULL,count,(void *)(p+i));
	}
	printf("\nWait threads\n");
	sleep(1);
	/* Wait for all threads. */
	int *x = malloc(sizeof(int));
	for (i=0; i<n; i++) {
		pthread_join(threads[i],(void*)x);
	}
	free(p);
	exit(0);
}
