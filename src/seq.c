/*
 * Description: 
 *     History: damonyang@tencent.com, 2013/02/24, create
 */

# include <stdint.h>
# include <unistd.h>
# include <fcntl.h>
# include <sys/stat.h>
# include <sys/mman.h>

# include "conf.h"
# include "utils.h"

static uint64_t *global_sequence;

void sequence_fini(void)
{
    munmap((void *)global_sequence, sizeof(uint64_t));
}

int sequence_init(void)
{
    if (global_sequence)
    {
        sequence_fini();
    }

    int fd = open(settings.global_sequence_file, O_RDWR | O_CREAT, 0777);
    if (fd < 0)
        return -__LINE__;

    struct stat st;
    if (fstat(fd, &st) < 0)
        return -__LINE__;

    if (st.st_size == 0)
    {
        uint64_t v = 0;
        write_in_full(fd, &v, sizeof(v));
    }

    void *addr = mmap(NULL, sizeof(uint64_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
        return -__LINE__;

    close(fd);

    global_sequence = (uint64_t *)addr;

    return 0;
}

uint64_t sequence_get(void)
{
    uint64_t v = ++(*global_sequence);
    msync((void *)global_sequence, sizeof(uint64_t), MS_ASYNC);

    return v;
}

