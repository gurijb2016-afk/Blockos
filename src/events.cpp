#include "events.hpp"

EventQueue g_event_queue;

bool EventQueue::push(const Event& ev) {
    int next = (tail + 1) % CAP;
    if (next == head) return false; // full
    buf[tail] = ev;
    tail = next;
    return true;
}

bool EventQueue::pop(Event& out) {
    if (head == tail) return false; // empty
    out = buf[head];
    head = (head + 1) % CAP;
    return true;
}
