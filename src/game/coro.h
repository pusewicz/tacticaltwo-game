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
//     int coro_state;     // Coroutine state (execution position)
//     int counter;        // Local variables must be in struct
//   } MyContext;
//
//   int my_sequence(MyContext* ctx) {
//     coro_begin(ctx->coro_state);
//
//     ctx->counter = 0;
//     while (ctx->counter < 3) {
//       do_something();
//       ctx->counter++;
//       coro_yield(ctx->coro_state);  // Resume here next call
//     }
//
//     coro_end(ctx->coro_state);
//   }
//
// Key points:
// - Local variables don't survive yields; store them in the context struct
// - Uses __COUNTER__ for unique case labels (GCC, Clang, MSVC)
// - Call coro_reset() before starting a new sequence
// - coro_done() returns true when the coroutine has finished

#pragma once

// Begin a coroutine block
#define coro_begin(state_var) \
  switch (state_var) {        \
  case 0:;

// Yield execution - next call resumes at this point
#define coro_yield(state_var) \
  do {                        \
    state_var = __COUNTER__;  \
    return 0;                 \
  case __COUNTER__:;          \
  } while (0)

// Yield until condition becomes true
#define coro_wait(state_var, condition) \
  do {                                  \
    state_var = __COUNTER__;            \
  case __COUNTER__:                     \
    if (!(condition))                   \
      return 0;                         \
  } while (0)

// End the coroutine block - sets state to -1 (done)
#define coro_end(state_var) \
  state_var = -1;           \
  }                         \
  return 0

// Reset coroutine to initial state
#define coro_reset(state_var) ((state_var) = 0)

// Check if coroutine has finished (reached coro_end)
#define coro_done(state_var) ((state_var) == -1)

// Check if coroutine is at the start (never run or just reset)
#define coro_idle(state_var) ((state_var) == 0)
