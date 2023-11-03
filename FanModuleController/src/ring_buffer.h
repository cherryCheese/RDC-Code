/*
 * ring_buffer.h
 *
 * Created: 8/14/2020 3:42:16 PM
 *  Author: E1210640
 */ 

#ifndef __RING_BUFFER__
#define __RING_BUFFER__

struct ring_buffer {
	int size;
	int head;
	int tail;
	uint8_t data[];
};

#define RING_BUFFER(_name, _size) \
	struct { \
		int size; \
		int head; \
		int tail; \
		uint8_t data[_size]; \
	} _name = { (_size), 0, 0, {0} }

static inline int ring_size(void *ptr)
{
	struct ring_buffer *ring = (struct ring_buffer *)ptr;
	
	return ring->head < ring->tail ? ring->size - ring->tail + ring->head : ring->head - ring->tail;
}

static inline void ring_put(void *ptr, uint8_t data)
{
	struct ring_buffer *ring = (struct ring_buffer *)ptr;
	
	if ((ring->head + 1) % ring->size != ring->tail) {
		ring->data[ring->head++] = data;
		ring->head %= ring->size;
	}
}

static inline uint8_t ring_get(void *ptr)
{
	struct ring_buffer *ring = (struct ring_buffer *)ptr;
	uint8_t ret = 0;
	
	if (ring->head != ring->tail) {
		ret = ring->data[ring->tail++];
		ring->tail %= ring->size;
	}
	
	return ret;
}

static inline int ring_get_buf(void *ptr, uint8_t *buf, int size)
{
	struct ring_buffer *ring = (struct ring_buffer *)ptr;
	int cnt = ring_size(ring) < size ? ring_size(ring) : size;
	int i;
	
	for (i = 0; i < cnt; i++) {
		buf[i] = ring_get(ring);
	}
	
	return cnt;
}

#endif /* __RING_BUFFER__ */