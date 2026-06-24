#ifndef WATCH_H
#define WATCH_H
typedef struct Watcher Watcher;
typedef void (*WatchFn)(const char *name, void *user);

Watcher *watch_create(const char *dir, WatchFn fn, void *user);
void watch_poll(Watcher *w);
void watch_destroy(Watcher *w); 
const char *watch_backend(Watcher *w);
#endif