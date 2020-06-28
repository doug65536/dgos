#pragma once
#include "types.h"
#include "mm.h"
#include "mutex.h"

// Reader takes whole buffer. Writer appends to current buffer,
// creating one whenever necessary
struct pipe_buffer_hdr_t {
    pipe_buffer_hdr_t *next;
    size_t size;
};

struct pipe_t {
    pipe_t();
    ~pipe_t();

    size_t capacity() const;
    bool reserve(size_t pages);
    ssize_t enqueue(void *data, size_t size, int64_t timeout);
    ssize_t dequeue(void *data, size_t size, int64_t timeout);

private:
    using lock_type = std::mutex;
    using scoped_lock = std::unique_lock<lock_type>;

    void cleanup_buffer(scoped_lock &lock);
    pipe_buffer_hdr_t *allocate_page(scoped_lock &lock);
    void free_page(pipe_buffer_hdr_t *page, scoped_lock &lock);

    // Points to buffer that is to hold more transmitted data
    pipe_buffer_hdr_t *write_buffer = nullptr;

    // Points to buffer
    pipe_buffer_hdr_t *read_buffer = nullptr;
    size_t read_ofs;

    // Free chain
    pipe_buffer_hdr_t *free_buffer = nullptr;

    // Bump allocator provides pages until page capacity is reached
    char *page_pool = nullptr;
    size_t page_bump = 0;
    size_t page_capacity = 0;

    lock_type pipe_lock;
    std::condition_variable pipe_not_empty;
    std::condition_variable pipe_not_full;
};
