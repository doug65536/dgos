#include "pipe.h"
#include "stdlib.h"
#include "export.h"

pipe_t::pipe_t()
{

}

pipe_t::~pipe_t()
{
    scoped_lock lock(pipe_lock);
    cleanup_buffer(lock);
}

size_t pipe_t::capacity() const
{
    return page_capacity << PAGE_SCALE;
}

void pipe_t::cleanup_buffer(scoped_lock& lock)
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

pipe_buffer_hdr_t *pipe_t::allocate_page(scoped_lock& lock,
                                                int64_t timeout_time)
{
    ext::cv_status wait_result = ext::cv_status::no_timeout;

    for (pipe_buffer_hdr_t *result = nullptr;
         page_pool && wait_result == ext::cv_status::no_timeout;
         wait_result = pipe_not_full.wait_until(lock, timeout_time)) {
        if (free_buffer) {
            // Use most recently used one from freelist
            result = free_buffer;
            free_buffer = free_buffer->next;
            result->next = nullptr;
            result->size = 0;
        } else if (page_bump < page_capacity) {
            // Haven't bump allocated them all yet
            result = new (page_pool + (page_bump * PAGESIZE))
                    pipe_buffer_hdr_t();

            ++page_bump;

            assert(page_bump <= page_capacity);
        } else {
            // Wait for someone to free a page
            continue;
        }

        return result;
    }

    // There are no buffers
    return nullptr;
}

void pipe_t::free_page(pipe_buffer_hdr_t *page, scoped_lock& lock)
{
    page->next = free_buffer;
    page->size = 0;
    free_buffer = page;
    pipe_not_full.notify_one();
}

bool pipe_t::reserve(size_t pages)
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

size_t pipe_t::overhead() const
{
    return sizeof(pipe_buffer_hdr_t);
}

ssize_t pipe_t::enqueue(void const *data, size_t size,
                               int64_t timeout_time)
{
    ssize_t sent = 0;

    // The payload capacity per page
    size_t const capacity = PAGESIZE - sizeof(*write_buffer);

    bool notify_pending = false;

    scoped_lock lock(pipe_lock);

    // Loop
    while (size) {
        if (!write_buffer || write_buffer->size == capacity) {
            // Need a new write buffer

            // We might wait to get a new page,
            // so push out any pending notify before waiting
            if (notify_pending) {
                pipe_not_empty.notify_all();
                notify_pending = false;
            }

            pipe_buffer_hdr_t *new_write_buffer;
            new_write_buffer = allocate_page(lock, timeout_time);

            // If timed out
            if (unlikely(!new_write_buffer))
                break;

            pipe_buffer_hdr_t **ptr_to_next_ptr = write_buffer
                    ? &write_buffer->next
                    : &write_buffer_first;

            *ptr_to_next_ptr = new_write_buffer;

            write_buffer = new_write_buffer;
        }

        // The amount of space left in this page
        size_t remain = capacity - write_buffer->size;

        // The beginning of the payload
        uint8_t *buf = (uint8_t*)(write_buffer + 1);

        // Compute the beginning of free space
        uint8_t *dest = buf + write_buffer->size;

        // Compute how much we can transfer on this loop
        size_t transferred = ext::min(remain, size);

        memcpy(dest, data, transferred);

        write_buffer->size += transferred;

        data = (char*)data + transferred;
        sent += transferred;
        size -= transferred;

        notify_pending = true;
    }

    lock.unlock();

    if (notify_pending)
        pipe_not_empty.notify_all();

    return sent;
}

ssize_t pipe_t::dequeue(void *data, size_t size, int64_t timeout_time)
{
    size_t received = 0;

    scoped_lock lock(pipe_lock);

    while (size) {
        if (read_buffer) {
            // Read serviced from existing read buffer chain

            size_t remain = read_buffer->size - read_ofs;

            size_t transferred = ext::min(remain, size);

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
            read_ofs = 0;

            write_buffer_first = nullptr;

            write_buffer = nullptr;
        } else {
            // Wait
            if (pipe_not_empty.wait_until(lock, timeout_time) ==
                    ext::cv_status::timeout)
                break;
        }
    }

    return received;
}
