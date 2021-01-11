#include "module_api.h"
#include "pmatomic.h"

#include <pthread.h>
#include <stdlib.h>
#include <uv.h>

#define XTM_FIFO_SIZE 16
#define XTM_FIFO_MAX_INPUT 8

struct msg {
	pthread_t self;
	unsigned long long counter;
};

static struct xtm_queue *q1;
static struct xtm_queue *q2;
static pthread_t t1;
static pthread_t t2;

static void
alloc_buffer_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	buf->base = (char*)malloc(suggested_size);
	buf->len = suggested_size;
}

static void
uv_read_pipe(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf)
{
	void *data[XTM_FIFO_SIZE];
	if (nread > 0) {
		struct xtm_queue *queue = ((uv_handle_t *)client)->data;
		unsigned count = xtm_msg_recv(queue, data, XTM_FIFO_SIZE);
		for (unsigned i = 0; i < count; i++) {
			struct msg *msg = (struct msg *)data[i];
			fprintf(stderr, "Thread %p received: %p %llu\n", pthread_self(), msg->self, msg->counter);
			free(msg);
		}
	}
	if (buf->base)
		free(buf->base);
}

static void
uv_enqueue_message(uv_timer_t* handle)
{
	static unsigned long long counter_1, counter_2;
	struct xtm_queue *queue = ((uv_handle_t *)handle)->data;
	struct msg *msg = (struct msg *)malloc(sizeof(struct msg));
	if (msg == NULL)
		return;
	msg->self = pthread_self();
	if (pthread_self() == t1) {
		msg->counter = counter_1++;
	} else if (pthread_self() == t2) {
		msg->counter = counter_2++;
	} else {
		free(msg);
		return;
	}
	xtm_msg_send(queue, msg);
}

void *
thread_func(void *data)
{
	uv_loop_t uvloop;
	uv_pipe_t pipe;
	int pipe_fd;
	uv_timer_t timer;
	struct xtm_queue *queue_in = NULL, *queue_out = NULL;

	if (data == (void *)1) {
		queue_in = q1 = xtm_create(XTM_FIFO_SIZE, XTM_FIFO_MAX_INPUT);
		while ((queue_out = q2) == NULL)
			;
		t1 = pthread_self();
	} else if (data == (void *)2) {
		queue_in = q2 = xtm_create(XTM_FIFO_SIZE, XTM_FIFO_MAX_INPUT);
		while ((queue_out = q1) == NULL)
			;
		t2 = pthread_self();
	}

	if (queue_in == NULL)
		goto finish;

	pipe_fd = xtm_fd(queue_in);
	uv_loop_init(&uvloop);
	uv_pipe_init(&uvloop, &pipe, 0);
	uv_pipe_open(&pipe, pipe_fd);
	((uv_handle_t *)&pipe)->data = queue_in;
	uv_read_start((uv_stream_t*)&pipe, alloc_buffer_cb, uv_read_pipe);
	uv_timer_init(&uvloop, &timer);
	uv_timer_start(&timer, uv_enqueue_message, 0, 100);
	((uv_handle_t *)&timer)->data = queue_out;
	uv_run(&uvloop, UV_RUN_DEFAULT);
	uv_timer_stop(&timer);
	uv_read_stop((uv_stream_t*)&pipe);
	uv_close((uv_handle_t *)&pipe, NULL);
	uv_loop_close(&uvloop);

finish:
	if (queue_in == NULL)
		xtm_delete(queue_in);
	return (void *)NULL;
}

int main()
{
	pthread_t thread_1, thread_2;
	if (pthread_create(&thread_1, NULL, thread_func, (void *)1) < 0)
		return EXIT_FAILURE;
	if (pthread_create(&thread_2, NULL, thread_func, (void *)2) < 0) {
		pthread_join(thread_1, NULL);
		return EXIT_FAILURE;
	}

	pthread_join(thread_1, NULL);
	pthread_join(thread_2, NULL);
	return EXIT_SUCCESS;
}