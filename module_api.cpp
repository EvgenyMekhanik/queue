#include "module_api.h"
#include "scsp_fifo.h"

#include <sys/eventfd.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>

#include <stdio.h>

struct msg {
	void (*fun)(void *);
	void *fun_arg;
};

struct xtm_queue {
	struct scsp_fifo<struct msg> *fifo;
	int n_input;
	int filedes[2];
};

struct xtm_queue *
xtm_create(unsigned size)
{
	struct xtm_queue *queue = (struct xtm_queue *)
		malloc(sizeof(struct xtm_queue));
	if (queue == NULL)
		goto fail_alloc_queue;
	queue->fifo = (struct scsp_fifo<struct msg> *)
		malloc(sizeof(struct scsp_fifo<struct msg>) +
		       size * sizeof(struct msg));
	if (queue->fifo == NULL)
		goto fail_alloc_fifo;
	if (pipe(queue->filedes) < 0)
		goto fail_alloc_fd;
	if (fcntl(queue->filedes[0], F_SETFL, O_NONBLOCK) < 0 ||
	    fcntl(queue->filedes[0], F_SETFL, O_NONBLOCK) < 0)
		goto fail_alloc_fd;
	if (scsp_fifo_init(queue->fifo, size) < 0) {
		errno = EINVAL;
		goto fail_fifo_init;
	}
	queue->n_input = 0;
	return queue;

fail_fifo_init:
	close(queue->filedes[0]);
	close(queue->filedes[1]);
fail_alloc_fd:
	free(queue->fifo);
fail_alloc_fifo:
	free(queue);
fail_alloc_queue:
	return NULL;
};

int
xtm_delete(struct xtm_queue *queue)
{
	int err = 0;
	if (close(queue->filedes[0]) < 0)
		err = errno;
	if (close(queue->filedes[1]) == 0)
		errno = err;
	free(queue->fifo);
	free(queue);
	return (errno == 0 ? 0 : -1);
}

int
xtm_msg_probe(struct xtm_queue *queue)
{
	if (scsp_fifo_free_count(queue->fifo) == 0) {
		errno = ENOBUFS;
		return -1;
	}
	return 0;
}

int
xtm_msg_notify(struct xtm_queue *queue)
{
	uint64_t tmp = 0;
	if (write(queue->filedes[1], &tmp, sizeof(tmp)) == sizeof(tmp))
		return 0;
	return -1;
}

int
xtm_fun_dispatch(struct xtm_queue *queue, void (*fun)(void *),
		 void *fun_arg, int delayed)
{
	struct msg msg;
	msg.fun = fun;
	msg.fun_arg = fun_arg;
	if (scsp_fifo_put(queue->fifo, &msg, 1) != 1) {
		errno = ENOBUFS;
		return -1;
	}
	if (pm_atomic_fetch_add(&queue->n_input, 1) == 0 && delayed == 0)
		return xtm_msg_notify(queue);
	return 0;
}

int
xtm_fd(const struct xtm_queue *queue)
{
	return queue->filedes[0];
}

unsigned
xtm_fun_invoke(struct xtm_queue *queue)
{
	uint64_t tmp;
	int save_errno = errno;
	while (read(queue->filedes[0], &tmp, sizeof(tmp)) > 0)
		;
	assert(errno == EAGAIN);
	errno = save_errno;
	unsigned cnt = scsp_fifo_execute(queue->fifo);
	pm_atomic_fetch_sub(&queue->n_input, cnt);
	return cnt;
}
