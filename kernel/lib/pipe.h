#pragma once
#include "types.h"
#include "mm.h"
#include "mutex.h"

// Reader takes whole buffer. Writer appends to current buffer,
// creating one whenever necessary
struct pipe_buffer_hdr_t {
    pipe_buffer_hdr_t *next = nullptr;
    size_t size = 0;
    uintptr_t reserved[6];
};

C_ASSERT_ISPO2(sizeof(pipe_buffer_hdr_t));

struct pipe_t {
    pipe_t();
    ~pipe_t();

    size_t capacity() const;

    bool reserve(size_t pages);

    // Get the number of bytes in each page that are not available for payload
    size_t overhead() const;

    ssize_t enqueue(void const *data, size_t size, int64_t timeout_time);

    ssize_t dequeue(void *data, size_t size, int64_t timeout_time);

private:
    using lock_type = std::mutex;
    using scoped_lock = std::unique_lock<lock_type>;

    void cleanup_buffer(scoped_lock &lock);

    pipe_buffer_hdr_t *allocate_page(scoped_lock &lock, int64_t timeout_time);

    void free_page(pipe_buffer_hdr_t *page, scoped_lock &lock);

    // Points to first page of buffered outgoing data
    pipe_buffer_hdr_t *write_buffer_first = nullptr;

    // Points to buffer that is to hold more transmitted data
    // Is equal to write_buffer_first sometimes, sometimes
    // it is a few next links ahead of write_buffer_first
    pipe_buffer_hdr_t *write_buffer = nullptr;

    // Points to buffer from which reads are serviced
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
