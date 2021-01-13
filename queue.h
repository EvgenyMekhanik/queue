#pragma once

#include "pmatomic.h"

struct msg {
	struct msg *next;
	void (*fun)(void *);
	void *fun_arg;
};

struct spsc_queue {
	struct msg *head;
	struct msg dummy;
};

static inline void
spsc_queue_init(struct spsc_queue *queue)
{
	queue->dummy.next = NULL;
	queue->head = &queue->dummy;
}

static inline void
spsc_queue_enqueue_one(struct spsc_queue *queue, struct msg *msg)
{
	struct msg *head = queue->head;
	queue->head->next = msg;
	pm_atomic_compare_exchange_strong(&queue->head, &head, msg);
}

static inline void
spsc_queue_dequeue_all(struct spsc_queue *queue, struct msg **msg)
{
	*msg = queue->dummy.next;
	queue->dummy.next = NULL;
	pm_atomic_store(&queue->head, &queue->dummy);
}

