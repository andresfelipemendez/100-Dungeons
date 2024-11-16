
#define EXPORT extern "C" __declspec(dllexport) __stdcall

typedef void (*void_func)(void);
typedef int (*int_pGame_func)(struct game *game);
typedef void (*void_pGame_func)(struct game *game);
typedef void (*void_pGamepChar_func)(struct game *game, const char *str);

#define HOTRELOAD_EVENT_NAME "Global\\ReloadEvent"

