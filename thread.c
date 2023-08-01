

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

//\在宏定义中表示一行
#define LL_ADD(item, list) do { 	\
	item->prev = NULL;				\
	item->next = list;				\
	list = item;					\
} while(0)

#define LL_REMOVE(item, list) do {						\
	if (item->prev != NULL) item->prev->next = item->next;	\
	if (item->next != NULL) item->next->prev = item->prev;	\
	if (list == item) list = item->next;					\
	item->prev = item->next = NULL;							\
} while(0)


typedef struct NWORKER {
	pthread_t thread;
	int terminate;
	struct NMANAGER *workqueue;
	struct NWORKER *prev;
	struct NWORKER *next;
} nWorker;

typedef struct NTASK {
	void (*task_function)(struct NTASK *task);
	void *user_data;
	struct NTASK *prev;
	struct NTASK *next;
} nTask;

typedef struct NMANAGER {
	struct NWORKER *workers;
	struct NTASK *waiting_tasks;
	pthread_mutex_t tasks_mtx;//互斥锁
	pthread_cond_t task_cond;//条件变量
} nThreadPool;

//typedef nWorkQueue nThreadPool;

static void *ntyWorkerThread(void *ptr) {
	nWorker *worker = (nWorker*)ptr;

	while (1) {
		pthread_mutex_lock(&worker->workqueue->tasks_mtx);

		while (worker->workqueue->waiting_tasks == NULL) {
			if (worker->terminate) break;
			pthread_cond_wait(&worker->workqueue->task_cond, &worker->workqueue->tasks_mtx);//对mtx先解锁，执行结束再加锁、看有没有任务
            //pthread_cond_signal()唤醒
		}

		if (worker->terminate) {
			pthread_mutex_unlock(&worker->workqueue->tasks_mtx);
			break;
		}

		nTask *task = worker->workqueue->waiting_tasks;
		if (task != NULL) {
			LL_REMOVE(task, worker->workqueue->waiting_tasks);//取任务
		}

		pthread_mutex_unlock(&worker->workqueue->tasks_mtx);

		if (task == NULL) continue;

		task->task_function(task);//执行任务
	}

	free(worker);
	pthread_exit(NULL);
}

//create
int ntyThreadPoolCreate(nThreadPool *workqueue, int numWorkers) {
    if (workqueue == NULL)return -1;
	if (numWorkers < 1) numWorkers = 1;
	memset(workqueue, 0, sizeof(nThreadPool));

	pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;
	memcpy(&workqueue->task_cond, &blank_cond, sizeof(workqueue->task_cond));//内存拷贝

	pthread_mutex_t blank_mutex = PTHREAD_MUTEX_INITIALIZER;
	memcpy(&workqueue->tasks_mtx, &blank_mutex, sizeof(workqueue->tasks_mtx));

	int i = 0;
	for (i = 0;i < numWorkers;i ++) {
		nWorker *worker = (nWorker*)malloc(sizeof(nWorker));
		if (worker == NULL) {
			perror("malloc");
			return 1;
		}

		memset(worker, 0, sizeof(nWorker));
		worker->workqueue = workqueue;

		//int ret = pthread_create(&worker->thread, NULL, ntyWorkerThread, (void *)worker);
		int ret = pthread_create(&worker->thread, NULL, ntyWorkerThread, worker);
		if (ret) {

			perror("pthread_create");
			free(worker);

			return 1;
		}

		LL_ADD(worker, worker->workqueue->workers);
	}

	return i;
}

//destory
void ntyThreadPoolShutdown(nThreadPool *workqueue) {
	nWorker *worker = NULL;

	for (worker = workqueue->workers;worker != NULL;worker = worker->next) {
		worker->terminate = 1;
	}

	pthread_mutex_lock(&workqueue->tasks_mtx);

	workqueue->workers = NULL;
	workqueue->waiting_tasks = NULL;

	pthread_cond_broadcast(&workqueue->task_cond);

	pthread_mutex_unlock(&workqueue->tasks_mtx);

}
//push
void ntyThreadPoolQueue(nThreadPool *workqueue, nTask *task) {

	pthread_mutex_lock(&workqueue->tasks_mtx);

	LL_ADD(task, workqueue->waiting_tasks);

	pthread_cond_signal(&workqueue->task_cond);
	pthread_mutex_unlock(&workqueue->tasks_mtx);

}

/*
#if 1
*/
#define MAX_THREAD			10
#define MAX_TASK		500

void king_counter(nTask *task) {

	int index = *(int*)task->user_data;

	printf("index : %d, selfid : %d\n", index, pthread_self());

	free(task->user_data);
	free(task);
}



int main(int argc, char *argv[]) {

	nThreadPool pool;

	ntyThreadPoolCreate(&pool, MAX_THREAD);

	int i = 0;
	for (i = 0;i < MAX_TASK;i ++) {
		nTask *task= (nTask*)malloc(sizeof(nTask));
		if (task == NULL) {
			perror("malloc");
			exit(1);
		}

		task->task_function = king_counter;
		task->user_data = malloc(sizeof(int));
		*(int*)task->user_data = i;

		ntyThreadPoolQueue(&pool, task);

	}

//        cout<<"jieshu"<<endl;
	getchar();
    return 0;

}
/*
#endif
*/
