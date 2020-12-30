#include "queue.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#define PRODUCER_COUNT 1
#define PRODUCER_MSG_MAX 100
#define CONSUMER_MSG_MAX 200
#define LOCK_FREE_FIFO_SIZE 1024

static struct lock_free_fifo *fifo;

struct thread_info {
	pthread_t self;
	int msg_number;
};

static void *
producer_main_f(void *arg)
{
	while (1) {
		void *data[PRODUCER_MSG_MAX];
		for (unsigned i = 0; i < PRODUCER_MSG_MAX; i++) {
			struct thread_info *info = malloc(sizeof(struct thread_info));
			if (!info)
				return (void *)NULL;
			info->self = pthread_self();
			info->msg_number = i;
			data[i] = info;
		}
		unsigned cnt = lock_free_fifo_put(fifo, data, PRODUCER_MSG_MAX);
		for (unsigned i = cnt; i < PRODUCER_MSG_MAX; i++) {
			free(data[i]);
		}
	}
}

static void *
consumer_main_f(void *arg)
{
	while (1) {
		void *data[CONSUMER_MSG_MAX];
		unsigned cnt = lock_free_fifo_get(fifo, data, CONSUMER_MSG_MAX);
		for (unsigned int i = 0; i < cnt; i++) {
			struct thread_info *info = data[i];
			free(data[i]);
		}
	}
}

int main()
{
	fifo = malloc(sizeof(struct lock_free_fifo) + LOCK_FREE_FIFO_SIZE * sizeof(void *));
	if (!fifo)
		return 0;
	lock_free_fifo_init(fifo, LOCK_FREE_FIFO_SIZE);
	pthread_t consumer, producer[PRODUCER_COUNT];
	for (unsigned int i = 0; i < PRODUCER_COUNT; i++)
		if (pthread_create(&producer[i], NULL, producer_main_f, NULL) < 0)
			return 0;

	if (pthread_create(&consumer, NULL, consumer_main_f, NULL) < 0)
		return 0;

	for(unsigned int i = 0; i < PRODUCER_COUNT; i++)
		pthread_join(producer[i], NULL);
	pthread_join(consumer, NULL);

	return 0;
}