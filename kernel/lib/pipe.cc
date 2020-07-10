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
    return 0;
}

EXPORT void pipe_t::cleanup_buffer(scoped_lock& lock)
{
    if (page_pool) {
        munmap(page_pool, page_capacity * PAGESIZE);

        page_pool = nullptr;
        page_bump = 0;
        page_capacity = 0;
        free_buffer = nullptr;
        write_buffer = nullptr;
        read_buffer = nullptr;
    }

    pipe_not_full.notify_all();
}

EXPORT pipe_buffer_hdr_t *pipe_t::allocate_page(scoped_lock& lock)
{
    for (pipe_buffer_hdr_t *result = nullptr;
         page_pool; pipe_not_full.wait(lock)) {
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

    // Loop
    while (size) {
        if (unlikely(!write_buffer)) {
            // Need a new write buffer

            write_buffer = allocate_page(lock);

            if (!read_buffer)
                read_buffer = free_buffer;

            write_buffer = free_buffer;
            free_buffer = free_buffer->next;
            write_buffer->next = nullptr;
            write_buffer->size = 0;
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
        size_t transferred = std::max(remain, size);

        memcpy(dest, data, transferred);

        write_buffer->size += transferred;

        data = (char*)data + transferred;
        size -= transferred;
    }

    return -int(errno_t::ENOSYS);
}

EXPORT ssize_t pipe_t::dequeue(void *data, size_t size, int64_t timeout)
{
    scoped_lock lock(pipe_lock);

    return -int(errno_t::ENOSYS);
}
