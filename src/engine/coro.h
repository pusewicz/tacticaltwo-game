// coro.h - Stackless Coroutines via Switch Statements
//
// A lightweight coroutine implementation using Duff's Device.
// Coroutines can yield execution and resume where they left off,
// making sequential animation logic easy to write.
//
// Based on Simon Tatham's coroutines and Scott Lembcke's state machines.
// https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html
// https://www.slembcke.net/blog/StateMachines/
//
// Usage:
//
//   typedef struct {
//     int coro;           // Coroutine state (must be named 'coro')
//     int counter;        // Local variables must be in struct
//   } MyContext;
//
//   void my_sequence(MyContext* ctx) {
//     coro_begin(ctx);
//
//     ctx->counter = 0;
//     while (ctx->counter < 3) {
//       do_something();
//       ctx->counter++;
//       coro_yield(ctx);  // Resume here next call
//     }
//
//     coro_end(ctx);
//   }
//
// Key points:
// - Local variables don't survive yields; store them in the context struct
// - The context must have an 'int coro' field (or use coro_begin_ex)
// - Call coro_reset() before starting a new sequence
// - coro_done() returns true when the coroutine has finished

#pragma once

// Begin a coroutine block. Context must have an 'int coro' field.
#define coro_begin(ctx)                                                        \
  switch ((ctx)->coro) {                                                       \
  case 0:

// Begin with explicit state field name
#define coro_begin_ex(state)                                                   \
  switch (state) {                                                             \
  case 0:

// Yield execution. Next call resumes at this point.
#define coro_yield(ctx)                                                        \
  do {                                                                         \
    (ctx)->coro = __LINE__;                                                    \
    return;                                                                    \
  case __LINE__:;                                                              \
  } while (0)

// Yield with explicit state field
#define coro_yield_ex(state)                                                   \
  do {                                                                         \
    (state) = __LINE__;                                                        \
    return;                                                                    \
  case __LINE__:;                                                              \
  } while (0)

// Yield until condition becomes true
#define coro_wait(ctx, condition)                                              \
  do {                                                                         \
    (ctx)->coro = __LINE__;                                                    \
  case __LINE__:                                                               \
    if (!(condition))                                                          \
      return;                                                                  \
  } while (0)

// Yield until condition becomes true (explicit state)
#define coro_wait_ex(state, condition)                                         \
  do {                                                                         \
    (state) = __LINE__;                                                        \
  case __LINE__:                                                               \
    if (!(condition))                                                          \
      return;                                                                  \
  } while (0)

// End the coroutine block. Sets state to -1 (done).
#define coro_end(ctx)                                                          \
  (ctx)->coro = -1;                                                            \
  }

// End with explicit state field
#define coro_end_ex(state)                                                     \
  (state) = -1;                                                                \
  }

// Reset coroutine to initial state
#define coro_reset(ctx) ((ctx)->coro = 0)

// Reset with explicit state field
#define coro_reset_ex(state) ((state) = 0)

// Check if coroutine has finished (reached coro_end)
#define coro_done(ctx) ((ctx)->coro == -1)

// Check with explicit state field
#define coro_done_ex(state) ((state) == -1)

// Check if coroutine is at the start (never run or just reset)
#define coro_idle(ctx) ((ctx)->coro == 0)

// Check with explicit state field
#define coro_idle_ex(state) ((state) == 0)
