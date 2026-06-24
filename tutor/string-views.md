# Strings With Length (string views) vs `char *`

A concept aside, not a build step. It underpins a lot of the data-oriented choices later (asset names, parsers, the reflection work in the roadmap), so it's worth understanding deeply before you reach for `char *` out of habit.

## What a `char *` actually is

A C string is a bare pointer to bytes with a `0` byte marking the end. The length is **not stored anywhere** — it's implied by the position of the terminator. That single design choice is the source of everything that follows.

To learn how long the string is, you must walk it byte by byte until you hit the `0`:

```c
size_t strlen(const char *s) {
    const char *p = s;
    while (*p) ++p;
    return p - s;
}
```

That is an **O(n) scan, every single time you need the length**. The CPU has to touch every byte, pulling cache lines it may not otherwise need. A loop that calls `strlen(s)` in its condition re-scans the whole string on every iteration — the classic accidental O(n²).

## What a string view is

A string view is a pointer **plus** an explicit length:

```c
typedef struct String {
    const char *data;
    size_t      len;
} String;
```

(Some codebases store `begin`/`end` pointers instead; same idea, length is `end - begin`.) The length is right there — known in O(1), no scan, no terminator required. This is what Rust's `&str`, Go's `string`, Zig's slices, and C++'s `std::string_view` all are under the hood: a fat pointer carrying its own size.

Everything below is a consequence of *carrying the length* and *not requiring a terminator*.

## Why it's better

**Length is free.** You never scan to find it. Comparisons short-circuit: two views of different length cannot be equal, so you reject them with one integer compare *without touching the bytes*. `strcmp` has no length, so it must walk both strings to discover they differ.

```c
bool sv_eq(String a, String b) {
    return a.len == b.len && SDL_memcmp(a.data, b.data, a.len) == 0;
}
```

**Substrings are free — no copy, no allocation.** A view can point *into* a larger buffer. Slicing is just arithmetic:

```c
String sv_slice(String s, size_t off, size_t n) {
    return (String){ s.data + off, n };
}
```

With `char *` you cannot do this. A substring of a C string has no `0` at its end (the next character is still part of the original), so to get a usable C string you must **copy the bytes out and append a terminator** — an allocation and a memcpy for every slice. This is the killer feature for parsers and tokenizers: a lexer over a config or shader file produces thousands of tokens, and with views each token is a `{ptr, len}` into the original source buffer — zero copies for the entire parse. Your file watcher's basenames could be views into the raw OS event buffer instead of copies.

**You can reference data you don't own and didn't terminate.** Memory-mapped files, network packets, a name sitting in the middle of a larger blob — all are length-delimited, none are NUL-terminated. A view describes a window into them directly. A `char *` API forces you to copy the region out and terminate it first.

**It's bounded, so it's safe.** The entire family of `str*` overruns exists because the functions run until they find a `0` that may not be there. `strcpy`, `strcat`, `gets` — you cannot tell them "stop at N bytes" because the length isn't part of the string. With a view, length travels *with* the data, so every operation is a bounded `memcpy(n)` / `memcmp(n)`. Bounds checks become possible because you have a bound.

**It can hold arbitrary bytes.** A length-delimited string can contain embedded `0` bytes and raw binary; a C string cannot, because the first `0` ends it. Useful the moment "text" and "bytes" start to blur (hashes, serialized blobs).

**It's friendlier to the cache and to batch processing (the DOD angle).** Store many strings as an array of `{ptr, len}` and you can compare, hash, or filter them using the lengths first — most work resolves without dereferencing into the character data at all, so you avoid the pointer-chasing scans that `strlen`/`strcmp` force. When you do hash, you hash exactly `len` bytes; no scanning to find the boundary.

## What it costs (be honest)

A view is two words instead of one, so it's bigger to pass around — though a 16-byte struct by value is cheap, and you can pass `const String *` if you care.

The real friction is the **boundary with the outside world**. The C standard library and every OS call want a NUL-terminated `const char *`: `fopen`, `open`, `inotify_add_watch`, `printf("%s")`. A view isn't guaranteed to be terminated (it may point into the middle of a buffer), so at that boundary you must materialize a terminated copy:

```c
char tmp[256];
SDL_snprintf(tmp, sizeof tmp, "%.*s", (int)sv.len, sv.data);
```

That `%.*s` — "print exactly this many bytes from this pointer" — is also how you print a view without first terminating it. The rule of thumb: **views internally, terminated `char *` only at the edges** where you hand strings to the OS or libc.

Finally, a view **does not own its memory**. It's a window; if the backing buffer is freed or goes out of scope, the view dangles. With a slice into a source file that lives for the whole parse this is a non-issue, but it means you must reason about lifetimes — a view is only valid as long as what it points at. An owning `char *` (or an owning `String` that allocated its own bytes) is clearer when the data must outlive its source.

## Why C ended up with the worse default (the history)

Length-prefixed strings are *older* than C. BCPL — C's direct ancestor — stored a string's length in its first cell. Multics PL/I and Pascal did the same. So the question isn't "why didn't anyone think of length-prefixing," it's "why did C drop it."

The answer is the hardware of the early 1970s. On a PDP-11 with kilobytes of memory, NUL-termination saved the byte(s) a length field would cost and needed no extra register to track a count — the terminator *was* the bookkeeping. Ritchie and Thompson optimized for the machine in front of them, and the `0`-terminated convention got welded into the language, the standard library, and eventually every OS ABI.

Poul-Henning Kamp later called this "The Most Expensive One-byte Mistake" (ACM Queue, 2011): trading one byte of storage for a *permanent* O(n) length operation and the single most fertile source of buffer-overflow vulnerabilities in computing history. The byte you saved in 1972 you've been repaying in CPU cycles and CVEs ever since. Every modern language reaching back to the length-prefixed design — Rust, Go, Zig, modern C++ — is, in effect, undoing that one optimization now that memory is no longer the binding constraint.

That's the deeper lesson and why it belongs in this set: the "default" data representation a language hands you encodes the trade-offs of its era's hardware. Writing data-oriented C means noticing when those baked-in defaults no longer match your machine, and choosing the representation that does.
