# `multiprocessing`

A Sere port of Python's `multiprocessing` package. The programming model is
Python's: `Process`, `Pipe`, `Lock`, `Event`, `Semaphore`, `cpu_count`,
`current_process`, start methods, contexts.

```sere
import multiprocessing as mp

def worker(name: str) -> void:
    print("hello", name)

// Registration runs during module initialization, so every process --
// including a spawned child -- performs it before running anything.
static _worker: bool = mp.expose("worker", worker)

def main() -> i32:
    p: mp.Process = mp.Process(target=worker, args=["Sere"])
    p.start()
    p.join()
    print(p.name, p.pid, p.exitcode)
    return 0
```

Arguments are passed positionally, as in Python: ``args=["Sere"]`` calls
``worker("Sere")``. A target may also take a single ``list[Any]`` parameter, in
which case it receives the whole argument list and casts each element itself:

```sere
def worker(args: list[Any]) -> void:
    print("hello", args[0] as str)
```

Positional parameters may be integers, ``bool``, ``str``, ``list`` or ``dict``.
A target that returns a value, or takes a float or other aggregate parameter,
must use the ``list[Any]`` form; the runtime rejects the other shapes with a
clear ``PicklingError`` rather than mis-calling the function.

## How `spawn` works here

Sere compiles a program to a *native executable*, so there is no module to
re-import in a child and no interpreter to re-run. Instead:

1. The parent frames a call as `(target name, encoded arguments)`.
2. The runtime starts a second copy of **the same executable** with
   `--sere-mp-child=<handle>` on its command line, handing it the payload over
   an inherited pipe.
3. The child's `main` runs the module initializers, then calls
   `sere_mp_bootstrap` before user `main`. In child mode that resolves the
   target by name, rebuilds the arguments and runs them, then exits with the
   target's status. User `main` is never entered, so an `if __name__ ==
   "__main__"` block cannot recurse.
4. The parent keeps a `PROCESS_INFORMATION` / `pid`, and `join`, `is_alive`,
   `exitcode`, `terminate`, `kill` map onto `WaitForSingleObject` /
   `GetExitCodeProcess` / `TerminateProcess` on Windows and `waitpid` /
   `kill` on POSIX.

No address, file descriptor or handle is ever reinterpreted across the
boundary: descriptors travel as their numeric value and are re-wrapped in the
child, and named OS objects are reopened by name.

## Deviations from Python

| Topic | Python | Sere | Why |
| --- | --- | --- | --- |
| Target | any picklable callable | a **free function registered by name** with `expose` | a child cannot reconstruct a closure's captured state |
| Arguments | `args=(...)`, `kwargs={...}` | `args=[...]`, passed positionally (or as one `list[Any]` parameter) | Sere is statically typed; positional parameters are integers, `bool`, `str`, `list` or `dict` |
| `kwargs` | supported | rejected with a clear error | no variadic or keyword matching at the call boundary |
| Subclassed `run()` | runs in the child | not supported for spawning | a child cannot rebuild an arbitrary subclass instance; use `target=` |
| Picklability | any picklable object | scalars, `str`, `list[T]`, `dict[str, T]`, `None`, and the package's own objects | classes and record types raise `PicklingError` instead of being copied by address |
| Empty list literal | `list` | `["a", "b"]` may infer `list[str]`; `args` normalizes anything list-shaped to `list[Any]` | Sere's inference; normalization happens in the runtime |
| `exitcode` | `None` while running | `None` while running | same |
| `ident` | the OS pid | same as `pid` | Sere has no separate thread identity for a process |

Two compiler quirks are worth knowing if you extend the package:

* A property must be *field-backed with the same name* (`@public name.get:`
  over a field `name`), otherwise the checker creates a synthetic field and
  reads of the property see the wrong storage.
* `isinstance(x, T)` narrows `x`. Casting the *narrowed* name reads the box as
  the narrowed type, so the package copies a boxed value to a fresh local
  before casting it. `isinstance(x, None)` is the supported `None` test for a
  boxed value; comparing a boxed `Any` with `is None` compares whole structs.

## Compatibility matrix

| Feature | Windows | Linux | macOS |
| --- | --- | --- | --- |
| `Process` / `spawn` | yes | implemented, not yet run | implemented, not yet run |
| `fork`, `forkserver` | no | no | no |
| `Pipe` / `Connection` | yes | implemented, not yet run | implemented, not yet run |
| `connection.wait` | yes (connections, processes) | connections only | connections only |
| `Lock` | yes | implemented, not yet run | implemented, not yet run |
| `Semaphore`, `BoundedSemaphore` | yes | implemented, not yet run | implemented, not yet run |
| `Event` | yes | implemented, not yet run | implemented, not yet run |
| `cpu_count` | yes | yes | yes |
| `current_process`, `active_children` | yes | yes | yes |
| `get_context`, start methods | yes | yes | yes |

"not yet run" means the code is written and compiled into the runtime but has
not been exercised on that platform. Only Windows was available for this work.

## Not implemented yet

These are absent rather than faked, because their semantics cannot be
approximated honestly:

* `Queue`, `SimpleQueue`, `JoinableQueue`
* `Pool`, `ApplyResult`, `AsyncResult`, `map` / `imap` / `starmap`, `ThreadPool`
* `RLock`, `Condition`, `Barrier`
* `shared_memory.SharedMemory`, `Value`, `Array`, `RawValue`, `RawArray`,
  `ShareableList`
* `Manager`, `BaseManager`, `SyncManager`, proxies, `Namespace`
* `Listener`, `Client`
* `resource_tracker`

The runtime layer they need is already in place and tested: named shared
memory, named semaphores and events, framed messages, wait-many, and the
reduction registry.
