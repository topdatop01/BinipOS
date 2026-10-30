#include <stdint.h>
#include <stddef.h>

struct limine_base_revision_request
{
    uint64_t id[2];
    uint64_t revision;
};

struct limine_base_revision_response
{
    uint64_t revision;
};

struct limine_terminal_request
{
    uint64_t id[2];
    uint64_t revision;
    uint64_t response;
};

struct limine_terminal
{
    uint64_t columns;
    uint64_t rows;
};

struct limine_terminal_response
{
    uint64_t revision;
    uint64_t terminal_count;
    struct limine_terminal **terminals;
    uint64_t write;
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_base_revision_request base_revision_request = 
{
    .id = { 0xf9562b2d5c916256, 0xa4d212d5d1b28f25 },
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_terminal_request terminal_request = 
{
    .id = { 0x078f74ae975d7750, 0xbc11a0d838b9417e },
    .revision = 0
};

size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len]) 
    {
        len++;
    }
    return len;
}

void print(const char *str)
{
    struct limine_terminal_response *resp = (struct limine_terminal_response *)terminal_request.response;
    
    if (resp == NULL || resp->terminal_count == 0) 
    {
        return; 
    }

    void (*limine_write)(struct limine_terminal *, const char *, uint64_t) = (void *)resp->write;

    limine_write(resp->terminals[0], str, strlen(str));
}

void _start(void)
{
    if (base_revision_request.revision == 0) 
    {
        __asm__ __volatile__ ("cli");
        for (;;)
        {
            __asm__ __volatile__ ("hlt");
        }
    }

    print("Hello World!");

    __asm__ __volatile__ ("cli");
    for (;;)
    {
        __asm__ __volatile__ ("hlt");
    }
}
