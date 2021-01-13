#include "module_api_c.h"
#include "queue.h"

#include <sys/eventfd.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>


struct xtm_queue {
	struct spsc_queue *fifo;
	int n_input;
	unsigned size;
	int filedes[2];
};

static inline bool
xtm_msg_notify(struct xtm_queue *queue)
{
	uint64_t tmp = 0;
	return (write(queue->filedes[1], &tmp, sizeof(tmp)) == sizeof(tmp));
}

struct xtm_queue *
xtm_create(unsigned size)
{
	struct xtm_queue *queue = (struct xtm_queue *)malloc(sizeof(struct xtm_queue));
	if (queue == NULL)
		goto fail_alloc_queue;
	queue->fifo = malloc(sizeof(struct spsc_queue));
	if (queue->fifo == NULL)
		goto fail_alloc_fifo;
	if (pipe(queue->filedes) < 0)
		goto fail_alloc_fd;
	if (fcntl(queue->filedes[0], F_SETFL, O_NONBLOCK) < 0 ||
	    fcntl(queue->filedes[0], F_SETFL, O_NONBLOCK) < 0)
		goto fail_alloc_fd;
	spsc_queue_init(queue->fifo);
	queue->n_input = 0;
	queue->size = size;
	return queue;

fail_alloc_fd:
	free(queue->fifo);
fail_alloc_fifo:
	free(queue);
fail_alloc_queue:
	return NULL;
};

void
xtm_delete(struct xtm_queue *queue)
{
	struct msg* msg, *tmp;
	close(queue->filedes[0]);
	close(queue->filedes[1]);
	spsc_queue_dequeue_all(queue->fifo, &msg);
	while(msg) {
		tmp = msg;
		msg = msg->next;
		free(tmp);
	}
	free(queue->fifo);
	free(queue);
}

int
xtm_fd(const struct xtm_queue *queue)
{
	return queue->filedes[0];
}

int
xtm_fun_dispatch(struct xtm_queue *queue, void (*fun)(void *), void *fun_arg, int delayed)
{
	if (pm_atomic_load(&queue->n_input) >= queue->size)
		return -1;
	struct msg *msg = (struct msg *)malloc(sizeof(struct msg));
	if (msg == NULL)
		return -1;
	msg->fun = fun;
	msg->fun_arg = fun_arg;
	msg->next = NULL;
	spsc_queue_enqueue_one(queue->fifo, msg);
	if (pm_atomic_fetch_add(&queue->n_input, 1) == 0 && delayed == 0)
		return xtm_msg_notify(queue);
	return true;
}

int
xtm_fun_invoke(struct xtm_queue *queue)
{
	struct msg* msg, *tmp;
	int cnt = 0;
	uint64_t unused;
	int save_errno = errno;

	while (read(queue->filedes[0], &unused, sizeof(unused)) > 0)
		;
	assert(errno == EAGAIN);
	errno = save_errno;
	spsc_queue_dequeue_all(queue->fifo, &msg);
	while (msg) {
		tmp = msg;
		msg = msg->next;
		tmp->fun(tmp->fun_arg);
		free(tmp);
		cnt++;
	}
	pm_atomic_fetch_sub(&queue->n_input, cnt);
	return cnt;
}