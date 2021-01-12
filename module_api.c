#include "module_api.h"
#include "fifo.h"

#include <sys/eventfd.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>


struct xtm_queue {
	struct lock_free_fifo *fifo;
	int n_input;
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
	queue->fifo = malloc(sizeof(struct lock_free_fifo) + size * sizeof(void *));
	if (queue->fifo == NULL)
		goto fail_alloc_fifo;
	if (pipe(queue->filedes) < 0)
		goto fail_alloc_fd;
	if (fcntl(queue->filedes[0], F_SETFL, O_NONBLOCK) < 0 ||
	    fcntl(queue->filedes[0], F_SETFL, O_NONBLOCK) < 0)
		goto fail_alloc_fd;
	if (lock_free_fifo_init(queue->fifo, size) < 0)
		goto fail_fifo_init;
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

void
xtm_delete(struct xtm_queue *queue)
{
	close(queue->filedes[0]);
	close(queue->filedes[1]);
	free(queue->fifo);
	free(queue);
}

int
xtm_fd(const struct xtm_queue *queue)
{
	return queue->filedes[0];
}

bool
xtm_msg_send(struct xtm_queue *queue, void *msg)
{
	if (lock_free_fifo_put(queue->fifo, &msg, 1) != 1)
		return false;
	if (pm_atomic_fetch_add(&queue->n_input, 1) == 0)
		return xtm_msg_notify(queue);
	return true;
}

unsigned
xtm_msg_recv(struct xtm_queue *queue, void **data, unsigned max_count)
{
	uint64_t tmp;
	int save_errno = errno;
	while (read(queue->filedes[0], &tmp, sizeof(tmp)) > 0)
		;
	assert(errno == EAGAIN);
	errno = save_errno;
	unsigned cnt = lock_free_fifo_get(queue->fifo, data, max_count);
	pm_atomic_fetch_sub(&queue->n_input, cnt);
	return cnt;
}