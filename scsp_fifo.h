#pragma once
/*
 * Copyright 2010-2020, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "pmatomic.h"

template<class T>
struct scsp_fifo {
	/*
	 * Next position to be written
	 */
	unsigned write;
	/*
	 * Next position to be read
	 */
	unsigned read;
	/*
	 * Circular buffer length
	 */
	unsigned len;
	/*
	 * Buffer contains pointers
	 */
	T buffer[];
};

template <class T>
static inline int
scsp_fifo_init(struct scsp_fifo<T> *fifo, unsigned size)
{
	/* Ensure size is power of 2 */
	if (size & (size - 1))
		return -1;

	fifo->write = 0;
	fifo->read = 0;
	fifo->len = size;
	return 0;
}

/*
 * Adds num elements into the fifo in case when we have one writing thread.
 * Return the number actually written.
 */
template <class T>
static inline unsigned
scsp_fifo_put(struct scsp_fifo<T> *fifo, T *data, unsigned num)
{
	unsigned i = 0;
	unsigned fifo_write = fifo->write;
	unsigned new_write = fifo_write;
	unsigned fifo_read = pm_atomic_load_explicit(&fifo->read, pm_memory_order_acquire);

	for (i = 0; i < num; i++) {
		new_write = (new_write + 1) & (fifo->len - 1);
		if (new_write == fifo_read)
			break;
		fifo->buffer[fifo_write] = data[i];
		fifo_write = new_write;
	}
	pm_atomic_store_explicit(&fifo->write, fifo_write, pm_memory_order_release);
	return i;
}

/*
 * Get up to num elements from the fifo. Return the number actually read
 */
template <class T>
static inline unsigned
scsp_fifo_get(struct scsp_fifo<T> *fifo, T *data, unsigned num)
{
	unsigned i = 0;
	unsigned new_read = fifo->read;
	unsigned fifo_write = pm_atomic_load_explicit(&fifo->write, pm_memory_order_acquire);

	for (i = 0; i < num; i++) {
		if (new_read == fifo_write)
			break;
		data[i] = fifo->buffer[new_read];
		new_read = (new_read + 1) & (fifo->len - 1);
	}
	pm_atomic_store_explicit(&fifo->read, new_read, pm_memory_order_release);
	return i;
}

/*
 * If T is a type that contains a function and a pointer to a function,
 * then this function for all queue elements of type T calls the functions
 * contained in the corresponding element with their arguments
 */
template <class T>
static inline unsigned
scsp_fifo_execute(struct scsp_fifo<T> *fifo)
{
	unsigned i = 0;
	unsigned new_read = fifo->read;
	unsigned fifo_write = pm_atomic_load_explicit(&fifo->write, pm_memory_order_acquire);

	for(;;) {
		if (new_read == fifo_write)
			break;
		fifo->buffer[new_read].fun(fifo->buffer[new_read].fun_arg);
		new_read = (new_read + 1) & (fifo->len - 1);
		i++;
	}
	pm_atomic_store_explicit(&fifo->read, new_read, pm_memory_order_release);
	return i;
}

/*
 * Get the num of elements in the fifo
 */
template <class T>
static inline unsigned
scsp_fifo_count(struct scsp_fifo<T> *fifo)
{
	unsigned fifo_write = pm_atomic_load_explicit(&fifo->write, pm_memory_order_acquire);
	unsigned fifo_read = pm_atomic_load_explicit(&fifo->read, pm_memory_order_acquire);
	return (fifo->len + fifo_write - fifo_read) & (fifo->len - 1);
}

/*
 * Get the num of available elements in the fifo
 */
template <class T>
static inline unsigned
scsp_fifo_free_count(struct scsp_fifo<T> *fifo)
{
	unsigned fifo_write = pm_atomic_load_explicit(&fifo->write, pm_memory_order_acquire);
	unsigned fifo_read = pm_atomic_load_explicit(&fifo->read, pm_memory_order_acquire);
	return (fifo_read - fifo_write - 1) & (fifo->len - 1);
}

