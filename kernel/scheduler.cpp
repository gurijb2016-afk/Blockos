#include "scheduler.hpp"

extern "C" {
#include <efi.h>
}
extern "C" {
#include <efilib.h>
}
#include <string.h>
#include <stdint.h>

namespace {

struct Task {
    int id;
    void (*entry)(void*);
    void* arg;

    // 0 = READY
    // 1 = RUNNING
    // 2 = FINISHED
    int state;

    Task* next;
};

static Task* runqueue_head = nullptr;
static int next_task_id = 1;

} // namespace

namespace scheduler {

void scheduler_init()
{
    runqueue_head = nullptr;
    next_task_id = 1;
}

int create_task(void (*entry)(void*), void* arg)
{
    if (!entry)
        return -1;

    Task* task = (Task*)AllocatePool(sizeof(Task));

    if (!task)
        return -1;

    memset(task, 0, sizeof(Task));

    task->id = next_task_id++;
    task->entry = entry;
    task->arg = arg;
    task->state = 0;
    task->next = nullptr;

    /*
     * Circular linked-list run queue.
     */
    if (!runqueue_head)
    {
        runqueue_head = task;
        task->next = task;
    }
    else
    {
        task->next = runqueue_head->next;
        runqueue_head->next = task;
    }

    /*
     * IMPORTANT:
     *
     * EDK2/GCC környezetben a CHAR16 formátumstringet
     * explicit módon CHAR16-ként adjuk meg.
     *
     * Így nem lesz:
     * invalid conversion from 'const wchar_t*'
     * to 'const CHAR16*'
     */
    CHAR16 buf[128];

    const CHAR16 format[] = {
        's','c','h','e','d','u','l','e','r',
        ':',' ','c','r','e','a','t','e','d',' ',
        't','a','s','k',' ',
        '%','d',
        '\n',
        0
    };

    UnicodeSPrint(
        buf,
        sizeof(buf),
        format,
        task->id
    );

    Print(buf);

    return task->id;
}

void yield()
{
    /*
     * Cooperative scheduler:
     * simply move to the next task.
     */
    if (runqueue_head && runqueue_head->next)
    {
        runqueue_head = runqueue_head->next;
    }
}

void run_scheduler_loop()
{
    if (!runqueue_head)
        return;

    Task* start = runqueue_head;
    Task* current = start;

    do
    {
        if (current->entry && current->state == 0)
        {
            current->state = 1;

            /*
             * Cooperative execution.
             * The task runs until it returns.
             */
            current->entry(current->arg);

            current->state = 2;
        }

        current = current->next;

    } while (current != start);
}

} // namespace scheduler
