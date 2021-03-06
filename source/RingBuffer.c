/*******************************************************/
/*              Includes                               */
/*******************************************************/

#include "rb/RingBuffer.h"
#include "rb/Utils.h"
#include "rb/priv/ErrorPriv.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*******************************************************/
/*              Defines                                */
/*******************************************************/

#define RING_BUFFER_MAGIC (0xA354BDDD )

#ifndef MIN
#define MIN(x, y) ( (x) < (y) ? (x) : (y) )
#endif

/*******************************************************/
/*              Typedefs                               */
/*******************************************************/

typedef struct {
    uint32_t head;
    uint32_t tail;
    uint32_t size;
} RingBufferBase;

typedef struct {
    uint32_t magic;
    uint8_t* buffer;
    RingBufferBase* base;
    int sharedMemory;
} RingBufferContext;

/*******************************************************/
/*              Functions Declarations                 */
/*******************************************************/

static const uint8_t* RingBufferPriv_getEnd(Rb_RingBufferHandle handle);

static uint8_t* RingBufferPriv_nextp(Rb_RingBufferHandle handle, const uint8_t *p);

static RingBufferContext* RingBufferPriv_getContext(Rb_RingBufferHandle handle);

/*******************************************************/
/*              Functions Definitions                  */
/*******************************************************/

Rb_RingBufferHandle Rb_RingBuffer_fromSharedMemory(void* vptr, uint32_t size,
        int init) {
    if(size == 0){
        RB_ERR("Invalid size");
        return NULL;
    }

    RingBufferContext* rb = (RingBufferContext*) RB_MALLOC(sizeof(RingBufferContext));
    memset(rb, 0x00, sizeof(RingBufferContext));

    uint8_t* data = (uint8_t*) vptr;

    rb->base = (RingBufferBase*) data;
    rb->base->size = size - sizeof(RingBufferBase);
    rb->buffer = data + sizeof(RingBufferBase);
    rb->magic = RING_BUFFER_MAGIC;
    rb->sharedMemory = 1;

    if(init) {
        Rb_RingBuffer_clear(rb);
    }

    return (Rb_RingBufferHandle) rb;
}

Rb_RingBufferHandle Rb_RingBuffer_new(uint32_t size) {
    if(size == 0){
        RB_ERR("Invalid size");
        return NULL;
    }

    RingBufferContext* rb = (RingBufferContext*) RB_CALLOC(sizeof(RingBufferContext));

    rb->base = (RingBufferBase*) RB_CALLOC(sizeof(RingBufferBase));
    // One byte is used for detecting the full condition.
    rb->base->size = size + 1;
    rb->buffer = (uint8_t*) RB_MALLOC(rb->base->size);
    rb->magic = RING_BUFFER_MAGIC;
    rb->sharedMemory = 0;

    Rb_RingBuffer_clear(rb);

    return (Rb_RingBufferHandle) rb;
}

int32_t Rb_RingBuffer_free(Rb_RingBufferHandle* handle) {
    RingBufferContext* rb = RingBufferPriv_getContext(*handle);
    if(rb == NULL) {
        RB_ERRC(RB_INVALID_ARG, "Invalid handle");
    }

    if(!rb->sharedMemory) {
        RB_FREE(&rb->buffer);
        RB_FREE(&rb->base);
    }

    RB_FREE(&rb);
    *handle = NULL;

    return RB_OK;
}

int32_t Rb_RingBuffer_clear(Rb_RingBufferHandle handle) {
    RingBufferContext* rb = RingBufferPriv_getContext(handle);
    if(rb == NULL) {
        RB_ERRC(RB_INVALID_ARG, "Invalid handle");
    }

    rb->base->head = rb->base->tail = 0;

    return 0;
}

int32_t Rb_RingBuffer_getCapacity(Rb_RingBufferHandle handle) {
    RingBufferContext* rb = RingBufferPriv_getContext(handle);
    if(rb == NULL) {
        RB_ERRC(RB_INVALID_ARG, "Invalid handle");
    }

    return rb->base->size - 1;
}

const uint8_t* RingBufferPriv_getEnd(Rb_RingBufferHandle handle) {
    RingBufferContext* rb = RingBufferPriv_getContext(handle);
    if(rb == NULL) {
        return NULL;
    }

    return rb->buffer + rb->base->size;
}

int32_t Rb_RingBuffer_getBytesFree(Rb_RingBufferHandle handle) {
    RingBufferContext* rb = RingBufferPriv_getContext(handle);
    if(rb == NULL) {
        RB_ERRC(RB_INVALID_ARG, "Invalid handle");
    }

    if(rb->base->head >= rb->base->tail) {
        return Rb_RingBuffer_getCapacity(rb) - (rb->base->head - rb->base->tail);
    } else {
        return rb->base->tail - rb->base->head - 1;
    }
}

int32_t Rb_RingBuffer_getBytesUsed(Rb_RingBufferHandle handle) {
    RingBufferContext* rb = RingBufferPriv_getContext(handle);
    if(rb == NULL) {
        RB_ERRC(RB_INVALID_ARG, "Invalid handle");
    }

    return Rb_RingBuffer_getCapacity(rb) - Rb_RingBuffer_getBytesFree(rb);
}

int32_t Rb_RingBuffer_isFull(Rb_RingBufferHandle handle) {
    RingBufferContext* rb = RingBufferPriv_getContext(handle);
    if(rb == NULL) {
        RB_ERRC(RB_INVALID_ARG, "Invalid handle");
    }

    return Rb_RingBuffer_getBytesFree(rb) == 0;
}

int32_t Rb_RingBuffer_isEmpty(Rb_RingBufferHandle handle) {
    RingBufferContext* rb = RingBufferPriv_getContext(handle);
    if(rb == NULL) {
        RB_ERRC(RB_INVALID_ARG, "Invalid handle");
    }

    return Rb_RingBuffer_getBytesFree(rb) == Rb_RingBuffer_getCapacity(rb);
}

uint8_t* RingBufferPriv_nextp(Rb_RingBufferHandle handle, const uint8_t *p) {
    RingBufferContext* rb = RingBufferPriv_getContext(handle);
    if(rb == NULL) {
        return NULL;
    }

    return rb->buffer + ((++p - rb->buffer) % rb->base->size);
}

int32_t Rb_RingBuffer_write(Rb_RingBufferHandle handle, const void *src,
        uint32_t count) {
    RingBufferContext* rb = RingBufferPriv_getContext(handle);
    if(rb == NULL) {
        RB_ERRC(RB_INVALID_ARG, "Invalid handle");
    }

    const uint8_t* u8src = (const uint8_t *) src;
    const uint8_t* bufend = RingBufferPriv_getEnd(rb);
    int overflow = (int32_t) count > Rb_RingBuffer_getBytesFree(rb);
    uint32_t nread = 0;

    while(nread != count) {
        // Don't copy beyond the end of the buffer
        uint32_t n = MIN(bufend - (rb->buffer + rb->base->head),
                (int32_t)(count - nread));
        memcpy((rb->buffer + rb->base->head), u8src + nread, n);
        rb->base->head += n;
        nread += n;

        // Wrap ?
        if((rb->buffer + rb->base->head) == bufend) {
            rb->base->head = 0;
        }
    }
    if(overflow) {
        rb->base->tail = RingBufferPriv_nextp(rb, (rb->buffer + rb->base->head))
                - rb->buffer;
    }

    return count;
}

int32_t Rb_RingBuffer_read(Rb_RingBufferHandle handle, void *dst, uint32_t count) {
    RingBufferContext* rb = RingBufferPriv_getContext(handle);
    if(rb == NULL) {
        RB_ERRC(RB_INVALID_ARG, "Invalid handle");
    }

    uint32_t bytes_used = Rb_RingBuffer_getBytesUsed(rb);
    if(count > bytes_used) {
        RB_ERRC(RB_INVALID_ARG, "Not enough data");
    }

    uint8_t *u8dst = (uint8_t *) dst;
    const uint8_t *bufend = RingBufferPriv_getEnd(rb);
    uint32_t nwritten = 0;

    while(nwritten != count) {
        uint32_t n = MIN(bufend - (rb->buffer + rb->base->tail),
                (int32_t)(count - nwritten));
        memcpy(u8dst + nwritten, (rb->buffer + rb->base->tail), n);
        rb->base->tail += n;
        nwritten += n;

        // Wrap?
        if((rb->buffer + rb->base->tail) == bufend) {
            rb->base->tail = 0;
        }
    }

    return count;
}

int32_t Rb_RingBuffer_resize(Rb_RingBufferHandle handle, uint32_t capacity) {
    RingBufferContext* rb = RingBufferPriv_getContext(handle);
    if (rb == NULL) {
        RB_ERRC(RB_INVALID_ARG, "Invalid handle");
    }

    if (rb->sharedMemory) {
        // Not yet implemented
        RB_ERRC(RB_NOT_IMPLEMENTED, "Not implemented");
    }

    if (capacity < rb->base->size) {
        // Shrinking not yet implemented
        RB_ERRC(RB_NOT_IMPLEMENTED, "Not implemented");
    }

    uint32_t previousBufferSize = rb->base->size;
    rb->base->size = capacity + 1;

    if (rb->base->tail > rb->base->head) {
        // Allocate new buffer
        uint8_t *newBuffer = (uint8_t*) RB_MALLOC(rb->base->size);

        uint32_t startPtr = previousBufferSize - rb->base->tail;

        // Copy data
        memcpy(newBuffer, rb->buffer + rb->base->tail, startPtr);
        memcpy(newBuffer + startPtr, rb->buffer, rb->base->head);

        startPtr = startPtr + rb->base->head;
        rb->base->head = startPtr;
        rb->base->tail = 0;

        // Free old buffer
        RB_FREE(&rb->buffer);

        // Assign new one
        rb->buffer = newBuffer;
    } else {
        rb->buffer = (uint8_t*) RB_REALLOC(rb->buffer, rb->base->size);
    }

    return RB_OK;
}

RingBufferContext* RingBufferPriv_getContext(Rb_RingBufferHandle handle) {
    if(handle == NULL) {
        return NULL;
    }

    RingBufferContext* rb = (RingBufferContext*) handle;
    if(rb->magic != RING_BUFFER_MAGIC) {
        return NULL;
    }

    return rb;
}
