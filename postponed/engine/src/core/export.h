#ifndef EXPORT_H
#define EXPORT_H


#ifdef __cplusplus
    #define EXPORT  __declspec(dllexport)  __stdcall
#else
    #define EXPORT __declspec(dllexport) __stdcall
#endif

typedef void (*void_func)(void);
typedef int (*int_pGame_func)(struct game *game);
typedef void (*void_pGame_func)(struct game *game);
typedef void (*void_pGamepChar_func)(struct game *game, const char *str);

#define HOTRELOAD_EVENT_NAME "Global\\ReloadEvent"

#endif
