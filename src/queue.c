/*
 * Description: A variable length circular queue, support single process or
 *              thread write and single process or thread read.
 *              Support use file as storage.
 *     History: damonyang@tencent.com, 2013/06/08, create
 */


# undef  _FILE_OFFSET_BITS
# define _FILE_OFFSET_BITS 64

# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# include <stdint.h>
# include <stdbool.h>
# include <assert.h>
# include <limits.h>
# include <errno.h>
# include <sys/types.h>
# include <sys/ipc.h>
# include <sys/shm.h>

# include "queue.h"

# define MAGIC_NUM 20130610

# pragma pack(1)

struct queue_head
{
    uint32_t magic;
    char     name[128];

    uint64_t shm_key;
    uint32_t mem_size;
    uint32_t mem_use;
    uint32_t mem_num;
    uint32_t p_head;
    uint32_t p_tail;

    char     file[512];
    uint64_t file_max_size;
    uint64_t file_start;
    uint64_t file_end;
    uint32_t file_num;
};

# pragma pack()

static void *__get_shm(key_t key, size_t size, int flag)
{
    int shm_id = shmget(key, size, flag);
    if (shm_id < 0)
        return NULL;

    void *p = shmat(shm_id, NULL, 0);
    if (p == (void *)-1)
        return NULL;

    return p;
}

static int get_shm(key_t key, size_t size, void **addr)
{
    if ((*addr = __get_shm(key, size, 0666)) != NULL)
        return 0;

    if ((*addr = __get_shm(key, size, 0666 | IPC_CREAT)) != NULL)
        return 1;

    return -1;
}

int queue_init(queue_t *queue, char *name, key_t shm_key,
        uint32_t mem_size, char *reserve_file, uint64_t file_max_size)
{
    if (!queue || !mem_size)
        return -2;

    size_t __mem_size = sizeof(struct queue_head) + mem_size;
    void *memory = NULL;
    bool old_shm = false;

    if (shm_key)
    {
        int ret = get_shm(shm_key, __mem_size, &memory);
        if (ret < 0)
            return -1;
        else if (ret == 0)
            old_shm = true;
    }
    else
    {
        memory = calloc(1, __mem_size);
        if (memory == NULL)
            return -1;
    }

    volatile struct queue_head *head = memory;

    if (old_shm == false)
    {
        head->magic    = MAGIC_NUM;

        if (name)
        {
            if (strlen(name) >= sizeof(head->name))
                return -3;
            strcpy((char *)head->name, name);
        }

        head->shm_key  = shm_key;
        head->mem_size = mem_size;

        if (reserve_file)
        {
            if (strlen(reserve_file) >= sizeof(head->file))
                return -4;
            strcpy((char *)head->file, reserve_file);

            remove((char *)head->file);
            errno = 0;

            head->file_max_size = file_max_size;
        }
    }
    else
    {
        if (name && strcmp((char *)head->name, name) != 0)
            return -5;
    }

    memset(queue, 0, sizeof(*queue));
    queue->memory = memory;

    return 0;
}

static int write_file(queue_t *queue, void *data, uint32_t size)
{
    volatile struct queue_head *head = queue->memory;

    if (head->file_max_size)
    {
        if ((head->file_end + (sizeof(size) + size)) > head->file_max_size)
            return -1;
    }

    if (head->file_num == UINT32_MAX)
        return -1;

    FILE *fp = fopen((char *)head->file, "a+");
    if (fp == NULL)
        return -3;

    if (fseeko(fp, head->file_end, SEEK_SET) != 0)
    {
        fclose(fp);

        return -4;
    }

    if (fwrite(&size, sizeof(size), 1, fp) != 1)
    {
        fclose(fp);

        return -5;
    }

    if (fwrite(data, size, 1, fp) != 1)
    {
        fclose(fp);

        return -6;
    }

    fclose(fp);

    head->file_end += (sizeof(size) + size);
    __sync_fetch_and_add(&head->file_num, 1);

    return 0;
}

static void putmem(queue_t *queue, uint32_t *p_tail, void *data, uint32_t size)
{
    volatile struct queue_head *head = queue->memory;
    void *buf = queue->memory + sizeof(struct queue_head);

    uint32_t tail_left = head->mem_size - *p_tail;

    if (tail_left < size)
    {
        memcpy(buf + *p_tail, data, tail_left);
        *p_tail = size - tail_left;
        memcpy(buf, data + tail_left, *p_tail);
    }
    else
    {
        memcpy(buf + *p_tail, data, size);
        *p_tail += size;
    }
}

int queue_push(queue_t *queue, void *data, uint32_t size)
{
    if (!queue || !data)
        return -2;

    volatile struct queue_head *head = queue->memory;
    assert(head->magic == MAGIC_NUM);

    if ((head->mem_size - head->mem_use) < (sizeof(size) + size))
    {
        if (head->file[0])
        {
            return write_file(queue, data, size);
        }

        return -1;
    }

    if (head->file[0] && head->file_end && head->file_num == 0)
    {
        remove((char *)head->file);

        head->file_start = 0;
        head->file_end   = 0;
    }

    uint32_t p_tail = head->p_tail;

    putmem(queue, &p_tail, &size, sizeof(size));
    putmem(queue, &p_tail, data, size);

    head->p_tail = p_tail;

    __sync_fetch_and_add(&head->mem_use, sizeof(size) + size);
    __sync_fetch_and_add(&head->mem_num, 1);

    return 0;
}

static void *alloc_read_buf(queue_t *queue, uint32_t size)
{
    if (queue->read_buf == NULL || queue->read_buf_size < size)
    {
        void  *buf = queue->read_buf;
        size_t buf_size = queue->read_buf_size;

        if (buf == NULL)
            buf_size = 1;

        while (buf_size < size)
            buf_size *= 2;

        buf = realloc(buf, buf_size);
        if (buf == NULL)
            return NULL;

        queue->read_buf = buf;
        queue->read_buf_size = buf_size;
    }

    return queue->read_buf;
}

static int read_file(queue_t *queue, void **data, uint32_t *size)
{
    volatile struct queue_head *head = queue->memory;

    errno = 0;
    FILE *fp = fopen((char *)head->file, "r");
    if (fp == NULL)
    {
        if (errno == ENOENT)
        {
            head->file_num   = 0;
            head->file_start = 0;
            head->file_end   = 0;
        }

        return -1;
    }

    if (fseeko(fp, head->file_start, SEEK_SET) != 0)
    {
        fclose(fp);

        return -2;
    }

    uint32_t __size = 0;
    if (fread(&__size, sizeof(__size), 1, fp) != 1)
    {
        fclose(fp);

        return -3;
    }

    *data = alloc_read_buf(queue, __size);
    if (*data == NULL)
    {
        fclose(fp);

        return -4;
    }

    if (fread(*data, __size, 1, fp) != 1)
    {
        fclose(fp);

        return -5;
    }

    fclose(fp);

    *size = __size;

    head->file_start += sizeof(__size) + __size;
    __sync_fetch_and_sub(&head->file_num, 1);

    return 0;
}

static void getmem(queue_t *queue, uint32_t *p_head, void *data, uint32_t size)
{
    volatile struct queue_head *head = queue->memory;
    void *buf = queue->memory + sizeof(struct queue_head);

    uint32_t tail_left = head->mem_size - *p_head;

    if (tail_left < size)
    {
        memcpy(data, buf + *p_head, tail_left);
        *p_head = size - tail_left;
        memcpy(data + tail_left, buf, *p_head);
    }
    else
    {
        memcpy(data, buf + *p_head, size);
        *p_head += size;
    }
}

static int check_mem(queue_t *queue, size_t size)
{
    volatile struct queue_head *head = queue->memory;

    if (head->mem_use < size)
    {
        head->mem_use = 0;
        head->mem_num = 0;
        head->p_head = head->p_tail = 0;

        return -1;
    }

    return 0;
}

int queue_pop(queue_t *queue, void **data, uint32_t *size)
{
    if (!queue || !data || !size)
        return -2;

    volatile struct queue_head *head = queue->memory;
    assert(head->magic == MAGIC_NUM);

    if (head->mem_num == 0)
    {
        if (head->file[0] && head->file_num)
        {
            int ret = read_file(queue, data, size);
            if (ret < 0)
                return -5 + ret;
            else
                return 0;
        }

        return -1;
    }

    uint32_t __size = 0;
    uint32_t p_head = head->p_head;

    if (check_mem(queue, sizeof(__size)) < 0)
        return -4;
    getmem(queue, &p_head, &__size, sizeof(__size));

    *data = alloc_read_buf(queue, __size);
    if (*data == NULL)
        return -3;
    *size = __size;

    if (check_mem(queue, (sizeof(__size) + __size)) < 0)
        return -5;
    getmem(queue, &p_head, *data, __size);

    head->p_head = p_head;

    __sync_fetch_and_sub(&head->mem_use, sizeof(__size) + __size);
    __sync_fetch_and_sub(&head->mem_num, 1);

    return 0;
}

uint64_t queue_len(queue_t *queue)
{
    if (!queue)
        return -2;

    volatile struct queue_head *head = queue->memory;
    assert(head->magic == MAGIC_NUM);

    return head->mem_use + head->file_end - head->file_start;
}

uint64_t queue_num(queue_t *queue)
{
    if (!queue)
        return -2;

    volatile struct queue_head *head = queue->memory;
    assert(head->magic == MAGIC_NUM);

    return head->mem_num + head->file_num;
}

int queue_stat(queue_t *queue, \
        uint32_t *mem_num, uint32_t *mem_size, uint32_t *file_num, uint64_t *file_size)
{
    volatile struct queue_head *head = queue->memory;
    assert(head->magic == MAGIC_NUM);

    *mem_num   = head->mem_num;
    *mem_size  = head->mem_use;
    *file_num  = head->file_num;
    *file_size = head->file_end - head->file_start;

    return 0;
}

void queue_fini(queue_t *queue)
{
    if (!queue)
        return;

    volatile struct queue_head *head = queue->memory;
    assert(head->magic == MAGIC_NUM);

    if (queue->read_buf)
        free(queue->read_buf);

    if (head->shm_key)
        shmdt(queue->memory);
    else
        free(queue->memory);

    return;
}

