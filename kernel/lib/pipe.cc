#include "pipe.h"
#include "stdlib.h"
#include "export.h"

EXPORT pipe_t::pipe_t()
{

}

EXPORT pipe_t::~pipe_t()
{
    scoped_lock lock(pipe_lock);
    cleanup_buffer(lock);
}

EXPORT size_t pipe_t::capacity() const
{
    return page_capacity << PAGE_SCALE;
}

EXPORT void pipe_t::cleanup_buffer(scoped_lock& lock)
{
    if (page_pool) {
        munmap(page_pool, page_capacity * PAGESIZE);

        page_pool = nullptr;
        page_bump = 0;
        page_capacity = 0;
        free_buffer = nullptr;
        write_buffer_first = nullptr;
        write_buffer = nullptr;
        read_buffer = nullptr;
    }

    pipe_not_full.notify_all();
}

EXPORT pipe_buffer_hdr_t *pipe_t::allocate_page(scoped_lock& lock,
                                                int64_t timeout)
{
    std::cv_status wait_result = std::cv_status::no_timeout;

    for (pipe_buffer_hdr_t *result = nullptr;
         page_pool && wait_result == std::cv_status::no_timeout;
         wait_result = pipe_not_full.wait_until(lock, timeout)) {
        if (free_buffer) {
            // Use most recently used one from freelist
            result = free_buffer;
            free_buffer = free_buffer->next;
            result->next = nullptr;
            result->size = 0;
        } else if (page_bump < page_capacity) {
            // Haven't bump allocated them all yet
            result = new (page_pool + (page_bump++ * PAGESIZE))
                    pipe_buffer_hdr_t();
        } else {
            // Wait for someone to free a page
            continue;
        }

        return result;
    }

    // There are no buffers
    return nullptr;
}

EXPORT void pipe_t::free_page(pipe_buffer_hdr_t *page, scoped_lock& lock)
{
    page->next = free_buffer;
    page->size = 0;
    free_buffer = page;
    pipe_not_full.notify_one();
}

EXPORT bool pipe_t::reserve(size_t pages)
{
    scoped_lock lock(pipe_lock);

    void *mem = mmap(nullptr, pages * PAGESIZE, PROT_READ | PROT_WRITE,
                     MAP_UNINITIALIZED | MAP_NOCOMMIT);

    if (unlikely(mem == MAP_FAILED))
        return false;

    cleanup_buffer(lock);

    page_pool = (char*)mem;
    page_capacity = pages;

    lock.unlock();
    pipe_not_full.notify_all();

    return true;
}

EXPORT ssize_t pipe_t::enqueue(void *data, size_t size, int64_t timeout)
{
    scoped_lock lock(pipe_lock);

    ssize_t sent = 0;

    // Loop
    while (size) {
        if (!write_buffer) {
            // Need a new write buffer

            write_buffer = allocate_page(lock, timeout);
            write_buffer_first = write_buffer;

            // Handle timeout
            if (unlikely(!write_buffer))
                return sent;
        }

        // The payload capacity per page
        size_t capacity = PAGESIZE - sizeof(*write_buffer);

        // The amount of space left in this page
        size_t remain = capacity - write_buffer->size;

        // The beginning of the payload
        uint8_t *buf = (uint8_t*)(write_buffer + 1);

        // Compute the beginning of free space
        uint8_t *dest = buf + write_buffer->size;

        // Compute how much we can transfer on this loop
        size_t transferred = std::min(remain, size);

        memcpy(dest, data, transferred);

        write_buffer->size += transferred;

        if (write_buffer->size == capacity) {
            // Buffer reached capacity, prepare another
            // And link it into the chain
            write_buffer->next = allocate_page(lock, timeout);

            // And bump ahead to new buffer
            write_buffer = write_buffer->next;
        }

        data = (char*)data + transferred;
        sent += transferred;
        size -= transferred;
    }

    return sent;
}

EXPORT ssize_t pipe_t::dequeue(void *data, size_t size, int64_t timeout)
{
    scoped_lock lock(pipe_lock);

    size_t received = 0;

    while (size) {
        if (read_buffer) {
            size_t remain = read_buffer->size - read_ofs;

            size_t transferred = std::min(remain, size);

            // The beginning of the payload
            uint8_t *buf = (uint8_t*)(read_buffer + 1);

            memcpy(data, buf + read_ofs, transferred);

            read_ofs += transferred;
            received += transferred;
            data = (char*)data + transferred;
            size -= transferred;

            if (read_ofs == read_buffer->size) {
                // Recycle the page

                pipe_buffer_hdr_t *freed_page = read_buffer;

                read_buffer = read_buffer->next;
                read_ofs = 0;

                free_page(freed_page, lock);
            }
        } else if (write_buffer) {
            // There is no read buffer, but something is buffered,
            // take the entire chain of pages from the writer
            read_buffer = write_buffer_first;

            write_buffer_first = nullptr;

            write_buffer = nullptr;
        } else {
            // Wait
            if (pipe_not_empty.wait_until(lock, timeout) ==
                    std::cv_status::timeout)
                return -int(errno_t::ETIMEDOUT);
        }
    }

    return received;
}
