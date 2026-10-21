# GYO trace transport and log facade

Owner: Engine (`engine/trace` → `GYO::Trace`, `engine/base/include/engine/base/Log.hpp`).
Plan and record: [time-threads-trace](plans/time-threads-trace/README.md), batch TT-2.

## What it is

```text
GYO::Base ◄── GYO::Time ◄── GYO::Threads ◄── GYO::Trace
   (Log.hpp)
```

- **`Engine::Trace::TraceSink`**: one file of pre-serialized lines from any number of producer threads. Producers
  append to a bounded queue under a short lock; one role thread (`Threads::RoleThread`) writes and flushes. The
  writer is woken by the Push that finds the queue empty (the role's `Time::Waiter` latches it), by `FlushNow` and by
  `Close`, never by a timer.
- **`Engine::Base::Log`**: level, category and text handed to one replaceable handler (an atomic function pointer,
  like `SetAssertionHandler`). The default writes one line to `std::cerr`. Where lines finally go is the consumer's
  choice.

Ownership: Engine owns the transport (queue, writer thread, flush, drop counts). The consumer owns record kinds,
content, schemas, file names, end lines and what a lost line means for it.

## Contracts

- `Push(line)` adds the newline and nothing else; the content is never parsed or rewritten.
- A Push that finds `capacity` lines waiting is dropped and counted (`droppedFull`); a Push after `Close` started is
  dropped and counted (`droppedClosed`). Nothing blocks a producer on the writer.
- `Close(endLine)` stops accepting, writes every accepted line, then the line `endLine(counts)` returns (if any),
  flushes and closes. `written` and `droppedFull` handed to `endLine` are final.
- `FlushNow(timeout)` blocks until every line accepted before the call is written and flushed. It returns false on
  the writer thread, after `Close`, after a failed write, or on timeout (including while another thread holds the
  queue: it only try-locks, so a crash path on a thread that holds the queue cannot hang); it is meant for paths
  about to end the process.
- `flushEachLine` makes the writer flush after every line, so each line reaches the file in one write; for files
  other processes may append to as well. The default flushes once per batch.
- `Good()` is false once a write or flush failed; drops do not make it false (the consumer decides what a drop
  means, for example by also requiring `Counts().Dropped() == 0`).

## Design Invariants

1. A producer's `Push` does no file I/O and never waits for the writer thread; a full queue drops and counts.
2. The writer thread is woken only by notification (Push into an empty queue, `FlushNow`, `Close`); it has no
   polling timer. Its wait has no deadline, and a wake for any other reason fails a `GYO_ASSERT`.
3. Conservation: once closed and no Push is running, pushes = `written` + `droppedFull` + `droppedClosed`. Every
   accepted line is written before the end line.
4. The end line is the last line of the file; nothing is written after it.
5. One producer's lines are written in its Push order; lines of different producers in the order they entered the
   queue.
6. Engine code never parses, rewrites or knows the meaning of a line: no consumer schema, record kind or file name
   enters `engine/`.
7. `GYO::Trace` links exactly `GYO::Threads` (checked at configure time by `tests/common/trace/CMakeLists.txt`): no
   SDL, no `GYO::Engine`.
8. The log facade never changes control flow and never throws; replacing the handler is safe at any time from any
   thread.

Guard tests: `tests/common/trace/TraceSinkTests.cpp` and the `tt2-*` mutants in
[mutations.json](plans/time-threads-trace/scripts/mutations.json).

### Replacement Criteria

Stop adding features and propose rebuilding instead of patching when any of these holds:

1. A consumer needs lines never to be lost (for example a record used for replay): the dropping queue would have to
   become blocking or spill to disk.
2. Adding a consumer requires re-verifying the close order or the counts.
3. Measurement shows the single queue lock of one sink is the main cost on a hot path.
