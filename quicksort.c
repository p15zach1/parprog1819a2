#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
//gcc -O2 -Wall -pthread quicksort.c -o qsort -DS=...-DN=... -DTHREAD_POOL=...

#define CUTOFF 100
#define LIMIT 1000

int work = 0; // ta paketa ergasias pou iparxoun
int queueSize = N;
bool keepRunning;

pthread_cond_t packet_in = PTHREAD_COND_INITIALIZER;
pthread_cond_t packet_completed = PTHREAD_COND_INITIALIZER;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct thread_params{
	double *a;
	int s;
};

struct thread_params queue[N];

struct thread_params completed[S/LIMIT];

void inssort(double *a, int s){
	double temp;
	int j;
	for(int i=1; i<s; i++){
		j=i;
		while((j>0) && (a[j-1] > a[j])){
			temp = a[j-1];
			a[j-1] = a[j];
			a[j] = temp;
			j--;
		}
	}
} 

int partition (double *a, int s){
	int first = 0;
	int middle = s/2;
	int last = s-1;
	double temp;
	double pivot = a[middle];
	int i, j;

	if(a[first]>a[middle]){
		temp = a[first];
		a[first] = a[middle];
		a[middle] = temp;
	}
	if(a[middle]>a[last]){
		temp = a[middle];
		a[middle] = a[last];
		a[last] = temp;
	}
	if(a[first]>a[middle]){
		temp = a[first];
		a[first] = a[middle];
		a[middle] = temp;
	}


	for (i=1, j=s-2 ;; i++, j--){	
		while(a[i]<pivot){
			i++;
		}
		while(a[j]>pivot){
			j--;
		}
		if (i >=j){
			break;
		}

		temp = a[i];
		a[i] = a[j];
		a[j] = temp;

	}
	return i;
}

void quicksort(double *a, int s){
	if(s<=CUTOFF){
		inssort(a, s);
		return;
	}

	int i;
	i = partition(a,s);

	quicksort(a, i);
	quicksort(a+i, s-i);
}

void queueR(){
	int i;
	for(i=1; i<N; i++){
		if(queue[i].a == NULL){
			queue[i-1] = queue[i];
			break;
		}else if(i==(N-1)){
			queue[i-1] = queue[i];
			queue[i].a = NULL;
			queue[i].s = -1;
			break;
		}
		queue[i-1] = queue[i];
	}
}

void completedR(){
	int i;
	for(i=1; i<N; i++){
		if(completed[i].a == NULL){
			completed[i-1] = completed[i];
			break;
		}else if(i==(N-1)){
			completed[i-1] = completed[i];
			completed[i].a = NULL;
			completed[i].s = -1;
			break;
		}
		completed[i-1] = completed[i];
	}
}

void * thread_func(void * params){
	struct thread_params tparm;
	bool pendingShutdown = false;
	int i, j;
	while(1){
		pthread_mutex_lock(&mutex);
		while(work<1 && !pendingShutdown){
			pthread_cond_wait(&packet_in, &mutex);
			pendingShutdown = !keepRunning;
		}
		if(pendingShutdown){
			break;
		}
		tparm.a = queue[0].a;
		tparm.s = queue[0].s;
		queueR();
		work--;
		
		if(tparm.s < LIMIT){
			quicksort(tparm.a, tparm.s);
			for(j=0; j<N; j++){
				if(completed[j].a == NULL){
					completed[j] = tparm;
					break;
				}
			}
			pthread_cond_signal(&packet_completed);
		}else{
			i = partition(tparm.a, tparm.s);

			struct thread_params tparm1, tparm2;
			tparm1.a = tparm.a;
			tparm1.s = i;

			tparm2.a = tparm.a + i;
			tparm2.s = tparm.s - i;

			for(j=0; j<N; j++){
				if(queue[j].a == NULL){
					queue[j].a = tparm1.a;
					queue[j].s = tparm1.s;
					queue[j+1].a = tparm2.a;
					queue[j+1].s = tparm2.s;
					work += 2;
					pthread_cond_signal(&packet_in);
					pthread_cond_signal(&packet_in);
					break;
				}
			}
		}

		pthread_mutex_unlock(&mutex);
	}
	pthread_mutex_unlock(&mutex);
	pthread_exit(NULL);
}

int main(){
	int i;
	int ordered = 0;
	double * a = (double*) malloc(S *  sizeof(double));
	srand(time(NULL));
	for(int i=0; i<N; i++){
		queue[i].a = NULL;
		queue[i].s = -1;
	}
	for(int i=0; i<S/LIMIT; i++){
		completed[i].a = NULL;
		completed[i].s = -1;
	}
  	for(int i=0; i<S; i++){
		a[i] = (double) rand()/RAND_MAX;
	}
	keepRunning = true;

	pthread_t mythread[THREAD_POOL];
	queue[0].a = a;
	queue[0].s = S;
	for(i=0; i<THREAD_POOL; i++){
		if(pthread_create(&mythread[i], NULL, thread_func, NULL) != 0){
			printf("create thread error!\n");
			exit(1);
		}
	}
	
	pthread_cond_signal(&packet_in);
	work++;
	while(1){
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&packet_completed, &mutex);
		while(1){
			if(completed[0].a != NULL){
				ordered += completed[0].s;
				completedR();
			} else{
				pthread_mutex_unlock(&mutex);
				break;
			}
		}
		if(ordered == S){
			sleep(1);
			keepRunning = false;
			for(i=0; i<THREAD_POOL; i++){
				pthread_cond_signal(&packet_in);
			}
			break;
		}
	}

	for(i=0; i<THREAD_POOL; i++){
		pthread_join(mythread[i], NULL);
	}

	for(i=0; i<(S-1); i++){
		if(a[i] > a[i+1]){
			printf("%lf, %lf\n", a[i], a[i+1]);
			printf("ERROR\n");
			break;
		}
	}

	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&packet_in);
	pthread_cond_destroy(&packet_completed);

	free(a);
	return 0;
}
