# OpenGL (`gl`)

`import gl` binds the OpenGL the Sere runtime exposes. The runtime
(`runtime/sere_gl.c`) owns a Win32 window, a WGL context, a `wglGetProcAddress`
loader for the modern entry points, and **184** `sere_gl_*` functions. The `gl`
package is a typed, documented wrapper over every one of them, split into parts
so the whole surface stays navigable.

- Quick reference and conventions: [`stdlib/gl/README.md`](../stdlib/gl/README.md)
- API source: [`stdlib/gl/`](../stdlib/gl/)
- Native layer: [`runtime/sere_gl.c`](../runtime/sere_gl.c),
  [`runtime/sere_rt.h`](../runtime/sere_rt.h)
- Generated enumerants: [`stdlib/gl/constants.sere`](../stdlib/gl/constants.sere)

| Section | What it covers |
| --- | --- |
| [Where it runs](#where-it-runs) | Availability, host support, threading |
| [Package layout](#package-layout) | What each `gl.*` module holds |
| [Conventions](#conventions) | Types, lists, booleans, contexts, errors |
| [A complete program](#a-complete-program) | Window, frame loop, immediate triangle |
| [Windows](#windows) | Flags, geometry, visibility, cursor, the frame loop |
| [Input](#input) | Keys, mouse, modifiers, text, clipboard |
| [Timing](#timing) | `time`, `dt`, frame-rate independence |
| [Pipeline state](#pipeline-state) | Clear, viewport, blending, depth, stencil, raster |
| [Immediate-mode drawing](#immediate-mode-drawing) | `begin`/`end` and the shape helpers |
| [Shaders](#shaders) | `Shader`, `Program`, uniforms, GLSL notes |
| [Matrices and 3D math](#matrices-and-3d-math) | Storage order, projections, cameras, transforms, picking |
| [Buffers and vertex arrays](#buffers-and-vertex-arrays) | VBOs, VAOs, attribute layouts, drawing |
| [Textures](#textures) | Uploads, filtering, wrapping, units |
| [Framebuffers](#framebuffers) | Off-screen rendering, completeness, renderbuffers |
| [Reading pixels back](#reading-pixels-back) | `read_rgba`, `pixel_at`, screenshots |
| [Queries](#queries) | Driver identity, extensions, limits |
| [Errors](#errors) | The error queue and how to drain it |
| [Recipes](#recipes) | Post-processing, resize handling, fixed timestep, screenshots |
| [Common enumerants](#common-enumerants) | The `GL_*` groups you actually use |
| [Gotchas](#gotchas) | The rules that bite |
| [Testing](#testing) | The feature test and how to run it |
| [Where the code lives](#where-the-code-lives) | Files, generators, regeneration |

## Where it runs

| Host | Behavior |
| --- | --- |
| Windows, `sere_rt` built with the display layer | Real windows, real driver, the whole API |
| Windows, no usable GL driver | Windows always loads its generic OpenGL 1.1 implementation, so `available()` is `True` and a context usually still appears; only the 1.1 subset works and the shader/buffer entry points are absent |
| Any other host | The runtime stubs every function: `available()` is `False`, window creation returns nothing, `is_open()` is `False`, queries answer zeroes |

```sere
import gl

def main() -> i32:
    print(gl.available(), gl.screen_width(), gl.screen_height())
    return 0
```

Two different questions, two different calls:

- `gl.available()` reports whether the runtime **has** a display layer at all.
  On Windows that is decided when `sere_rt` is built, not by the hardware or the
  driver, so it is not a capability probe.
- `window.is_open()` reports whether a window **and a context** actually came
  up. This is the call to branch on.

```sere
import gl

def main() -> i32:
    window: gl.Window = gl.Window("probe", 320, 200, gl.WINDOW_VISIBLE)
    if not window.is_open():
        print("no context on this host")
        return 1
    window.make_current()
    print(gl.version(), gl.vendor(), gl.renderer())
    window.destroy()
    return 0
```

**Threading.** A WGL context is current only on the thread that called
`make_current()`. Create the window, make it current, and drive the loop from one
thread. The runtime has no context-sharing or multi-window machinery: one window
per process is the supported shape.

**Nothing raises for GL itself.** A failing call queues an error code rather than
throwing; see [Errors](#errors).

## Package layout

| Module | Contents |
| --- | --- |
| `gl.constants` | 520 enumerants: the `GL_*` groups, `WINDOW_*` flag bits, `CURSOR_*`, `MOUSE_*`, `MOD_*`, and 106 `KEY_*` virtual-key codes |
| `gl.types` | `GLenum`, `GLbitfield`, `GLuint`, `GLint`, `GLsizei`, `GLfloat`, `GLintptr`, `GLhandle`, `VertexData`, `IndexData`, `PixelData`, `PixelRow` |
| `gl.info` | `available`, driver identity, extension list, display size, numeric queries |
| `gl.errors` | `take_error`, `error_name`, `error`, `require`, `pending`, `clear_all` |
| `gl.state` | Clear, viewport, capabilities, blending, depth/stencil, rasterisation, pixel store, hints, sync |
| `gl.immediate` | `begin`/`end` and `vertex`, `color`, `rect`, `line`, `triangle`, `circle` |
| `gl.shaders` | `Shader`, `Program`, uniforms, `build` |
| `gl.buffers` | `Buffer`, `VertexArray`, attribute layouts, `draw_arrays`, `draw_elements` |
| `gl.textures` | `Texture`, filtering/wrapping helpers, uploads, `rgba_pixels` |
| `gl.framebuffer` | `Framebuffer`, `Renderbuffer`, `blit_framebuffer`, `read_pixels` |
| `gl.window` | `Window`: creation, the frame loop, input, timing, cursor, clipboard |
| `gl.transform` | Projections, views, `Camera`, `Transform`, matrix uniform uploads, screen space, picking |

`gl/__init__.sere` is a generated barrel that imports every part by name and
re-exports it, so `import gl` reaches all of it. Reach for `gl.window` and
friends only if you want to import a single part by name:

```sere ignore
from gl.window import Window
from gl.constants import GL_COLOR_BUFFER_BIT
```

Every module works two ways: as `gl.name` after `import gl`, or as a bare name
when you import a single part. A program that imports parts by name never needs
the barrel, and therefore never pays for parsing it:

```sere
from gl.window import Window
from gl.constants import GL_COLOR_BUFFER_BIT
from gl.state import clear, clear_color

def main() -> i32:
    window: Window = Window("part", 320, 200)
    if not window.is_open():
        return 1
    window.make_current()

    clear_color(0.1, 0.1, 0.12, 1.0)
    clear(GL_COLOR_BUFFER_BIT)
    window.swap()
    window.destroy()
    return 0
```

## Conventions

| GL concept | Sere type | Notes |
| --- | --- | --- |
| Enumerant, mask, object name | `u32` | The enum parameters are `uint32_t` in the runtime |
| Count, size, level, location, reference | `i32` | |
| Byte offset, element count in bytes | `i64` | Strides and offsets are always bytes, never elements |
| Scalar | `f64` | The runtime narrows to `float` at the call |
| Data block | `list[f64]`, `list[i32]`, `list[byte]` | The runtime reads the list's element width, so element type matters |
| Pixel readback | `array[i8]` | One fixed allocation the runtime fills |
| Window handle | `Ptr[i8]` | Hidden inside `Window`; only needed if you call the native layer yourself |

Signatures follow the specification one-for-one: `gl.tex_image2d(...)` corresponds
to `glTexImage2D`, and the enumerants keep their `GL_` prefix
(`gl.GL_TEXTURE_2D`), PyOpenGL style.

**Boolean-valued driver calls return `bool`.** Wrappers around calls that set an
integer flag map it for you, so no `!= 0` is needed:

```sere
import gl

def main() -> i32:
    window: gl.Window = gl.Window("state", 200, 150, gl.WINDOW_VISIBLE)
    if not window.is_open():
        return 1
    window.make_current()

    gl.enable(gl.GL_DEPTH_TEST)
    gl.depth_func(gl.GL_LESS)
    assert gl.is_enabled(gl.GL_DEPTH_TEST)
    gl.disable(gl.GL_DEPTH_TEST)
    assert not gl.is_enabled(gl.GL_DEPTH_TEST)

    assert window.vsync() or not window.vsync()   # both are bools
    window.destroy()
    return 0
```

**No context, no work.** Every call is safe before `make_current()`: the runtime
guards the driver pointers it loads, and a GL call with no current context is a
no-op by the specification. Object creation then returns `0` and queries return
zeroes. That is why a program can test `available()` and a few limits before it
has a window.

**Errors are queued, not raised.** A call that fails leaves a flag; read it with
`gl.take_error()` or turn it into an exception with `gl.require("operation")`.
See [Errors](#errors).

**Ownership is explicit.** Buffers, textures, shaders, programs, framebuffers,
and renderbuffers are driver-side names with no finalizer: call `destroy()` (or
the matching `delete_*`). A `Window` owns its context and must be destroyed too.

## A complete program

This is the shortest useful graphics program: create a window, clear it, draw a
colour-interpolated triangle with the fixed-function pipeline, and present.

```sere
import gl

def main() -> i32:
    window: gl.Window = gl.Window("triangle", 640, 360)
    if not window.is_open():
        print("no display available")
        return 1

    window.make_current()
    gl.default_depth_state()

    while window.is_open():
        window.poll()
        if window.should_close() or window.key_pressed(gl.KEY_ESCAPE):
            window.destroy()

        gl.viewport(0, 0, window.framebuffer_width(), window.framebuffer_height())
        gl.clear_color(0.08, 0.09, 0.12, 1.0)
        gl.clear(gl.GL_COLOR_BUFFER_BIT)

        gl.begin(gl.GL_TRIANGLES)
        gl.color3f(1.0, 0.35, 0.3)
        gl.vertex2f(0.0, 0.7)
        gl.color3f(0.3, 1.0, 0.5)
        gl.vertex2f(-0.7, -0.6)
        gl.color3f(0.3, 0.5, 1.0)
        gl.vertex2f(0.7, -0.6)
        gl.end()

        window.swap()

    return 0
```

Compile and run it like any other program:

```
sere run
```

The shape of the loop is worth internalising, because everything else in this
page hangs off it:

1. `poll()` — consume the events that arrived since the last frame. Nothing is
   delivered until you ask.
2. Read input and decide whether to stop. `destroy()` sets the handle to nothing,
   which is what ends `while window.is_open()`.
3. Draw a frame. `viewport` is set every frame so resizes are handled for free.
4. `swap()` — present, blocking on vsync when it is enabled.

## Windows

`gl.Window` wraps the runtime's window and context: creation, the event loop,
geometry, appearance, visibility, focus, the cursor, and the clipboard.

```sere ignore
window: gl.Window = gl.Window("title", 960, 540)
```

The constructor takes a title, a client size in logical pixels, and an optional
flag bit set. The default is `gl.WINDOW_DEFAULT`.

| Flag | Value | Effect |
| --- | --- | --- |
| `WINDOW_VISIBLE` | `1` | Show the window when it is created |
| `WINDOW_RESIZABLE` | `2` | Let the user resize it |
| `WINDOW_DECORATED` | `4` | Draw the title bar and border |
| `WINDOW_MAXIMIZED` | `8` | Start maximised |
| `WINDOW_FLOATING` | `16` | Keep it above windows that are not floating |
| `WINDOW_FOCUSED` | `32` | Take keyboard focus on creation |
| `WINDOW_FULLSCREEN` | `64` | Start fullscreen |
| `WINDOW_VSYNC` | `128` | Enable vertical sync at creation |
| `WINDOW_DEFAULT` | `167` | `VISIBLE + RESIZABLE + DECORATED + FOCUSED + VSYNC` |

Combine flags with `+`. Passing a set explicitly replaces the default, so the
window below has no title bar and starts hidden:

```sere
import gl

def main() -> i32:
    hidden: gl.Window = gl.Window("hidden", 320, 200, gl.WINDOW_RESIZABLE)
    if not hidden.is_open():
        return 1
    hidden.make_current()

    # A hidden window still has a context: render, read, then show it.
    hidden.show()
    print(hidden.visible(), hidden.resizable(), hidden.decorated())
    hidden.destroy()

    # Fullscreen with vsync, and a maximised window with the default flags.
    game: gl.Window = gl.Window("game", 1280, 720, gl.WINDOW_FULLSCREEN + gl.WINDOW_VSYNC)
    print(game.fullscreen(), game.vsync())
    game.destroy()
    return 0
```

### The event loop

| Method | Result |
| --- | --- |
| `is_open()` | Window exists, has a context, and was not asked to close |
| `should_close()` | The user clicked the close box, or the window is already gone |
| `close()` | Ask the loop to stop; `should_close()` turns `True` |
| `destroy()` | Release the context, the device context, and the window. Required |
| `poll()` | Process what has arrived; returns how many events were handled |
| `wait()` | Block until at least one event arrives, then process it |
| `swap()` | Present the back buffer |
| `make_current()` | Bind the context to the calling thread |

`wait()` is the idle-friendly alternative to `poll()`: it sleeps instead of
spinning. A loop that renders continuously wants `poll()`; a tool that redraws
only on input wants `wait()`.

`destroy()` is idempotent-safe: it clears the handle, so `is_open()` is `False`
afterwards and a second call does nothing.

### Geometry

| Method | Result |
| --- | --- |
| `width()`, `height()` | Client area in logical pixels |
| `framebuffer_width()`, `framebuffer_height()` | Client area in physical pixels — use these for the viewport |
| `size()`, `framebuffer_size()` | The same pairs as `list[i32]` |
| `x()`, `y()` | Position on the desktop |
| `set_size(width, height)` | Resize the client area |
| `set_position(x, y)` | Move the window |
| `set_min_size(width, height)` | Smallest size the user may drag to; `0` for no limit |
| `set_max_size(width, height)` | Largest size the user may drag to; `0` for no limit |
| `content_scale()` | Physical pixels per logical pixel, `2.0` on a 200% display |
| `resized()` | The window was resized since the previous frame |

On a HiDPI display `width()` and `framebuffer_width()` differ. Drawing in logical
units and setting the viewport from the framebuffer size is the combination that
keeps the image sharp and the mouse coordinates consistent, because
`mouse_x()`/`mouse_y()` are logical too.

### Appearance and window management

| Method | Result |
| --- | --- |
| `set_title(text)`, `title()` | Title bar text |
| `set_vsync(bool)`, `vsync()` | Vertical sync; caps the frame rate at the refresh rate |
| `set_fullscreen(bool)`, `fullscreen()` | Fullscreen state |
| `set_resizable(bool)`, `resizable()` | Whether the user may resize |
| `set_decorated(bool)`, `decorated()` | Title bar and border |
| `set_floating(bool)`, `floating()` | Always-above hint |
| `set_opacity(0.0…1.0)`, `opacity()` | Whole-window opacity |
| `show()`, `hide()` | Visibility without destroying the context |
| `minimize()`, `maximize()`, `restore()` | Window size state |
| `focus()`, `focused()` | Take or read keyboard focus |
| `request_attention()` | Ask the window manager to flash the taskbar entry |
| `visible()`, `hovered()`, `minimized()`, `maximized()` | State predicates |

### Cursor and clipboard

| Method | Result |
| --- | --- |
| `show_cursor(bool)` | Show or hide the pointer |
| `set_cursor_mode(mode)`, `cursor_mode()` | `CURSOR_NORMAL`, `CURSOR_HIDDEN`, `CURSOR_DISABLED`, or `CURSOR_CAPTURED` |
| `set_cursor_position(x, y)` | Warp the pointer inside the window |
| `clipboard()`, `set_clipboard(text)` | The window's clipboard text |

| Cursor mode | Value | Effect |
| --- | --- | --- |
| `CURSOR_NORMAL` | `0` | Visible and free |
| `CURSOR_HIDDEN` | `1` | Hidden, still free, movement keeps being reported |
| `CURSOR_DISABLED` | `2` | Hidden and locked in place; deltas keep reporting motion — use this for mouse look |
| `CURSOR_CAPTURED` | `3` | Hidden and dragged along, so the pointer stays inside the window |

```sere
import gl

def main() -> i32:
    window: gl.Window = gl.Window("mouse look", 640, 360)
    if not window.is_open():
        return 1
    window.make_current()
    window.set_cursor_mode(gl.CURSOR_DISABLED)
    print(gl.cursor_mode_name(window.cursor_mode()))

    while window.is_open():
        window.poll()
        if window.should_close():
            window.destroy()
        # Deltas are pixels moved this frame, and keep counting in DISABLED mode.
        yaw: i32 = window.mouse_dx()
        pitch: i32 = window.mouse_dy()
        if yaw != 0 or pitch != 0:
            print(yaw, pitch)
        window.swap()

    return 0
```

## Input

Input is a per-frame snapshot: `poll()` (or `wait()`) updates it, and the
"pressed" and "released" predicates are true only for the frame in which the
transition happened.

| Method | Result |
| --- | --- |
| `key(code)` | A key is held down |
| `key_pressed(code)` | The key went down during this frame — the one for toggles and one-shot actions |
| `key_released(code)` | The key came up during this frame |
| `mods()` | `MOD_*` bits for the modifiers currently held |
| `modifier(mod)` | One `MOD_*` bit is set |
| `typed_char()` | Code point typed during this frame, `0` when none — for text entry |
| `mouse_x()`, `mouse_y()` | Cursor position inside the window, logical pixels |
| `mouse_position()` | The same as `list[i32]` |
| `mouse_dx()`, `mouse_dy()` | Movement since the previous frame |
| `mouse_delta()` | The same as `list[i32]` |
| `mouse_button(button)` | A button is held |
| `mouse_pressed(button)`, `mouse_released(button)` | The button changed state this frame |
| `wheel()` | Wheel detents scrolled this frame, positive for up |

Keys are Windows virtual-key codes, so `gl.KEY_A` is the physical A key on a
QWERTY layout and `gl.KEY_Z` is where your layout puts Z. `typed_char()` is the
one to use for text, because it reports what the keyboard actually produced.

| Group | Names |
| --- | --- |
| Editing | `KEY_BACKSPACE`, `KEY_TAB`, `KEY_ENTER`, `KEY_ESCAPE`, `KEY_DELETE`, `KEY_INSERT`, `KEY_SPACE` |
| Arrows and navigation | `KEY_LEFT`, `KEY_RIGHT`, `KEY_UP`, `KEY_DOWN`, `KEY_HOME`, `KEY_END`, `KEY_PAGE_UP`, `KEY_PAGE_DOWN` |
| Modifiers | `KEY_SHIFT`, `KEY_CONTROL`, `KEY_ALT`, `KEY_LEFT_SHIFT`, `KEY_RIGHT_SHIFT`, `KEY_LEFT_CONTROL`, `KEY_RIGHT_CONTROL`, `KEY_LEFT_ALT`, `KEY_RIGHT_ALT`, `KEY_LEFT_SUPER`, `KEY_RIGHT_SUPER` |
| Locks and function keys | `KEY_CAPS_LOCK`, `KEY_NUM_LOCK`, `KEY_SCROLL_LOCK`, `KEY_F1` … `KEY_F12` |
| Letters and digits | `KEY_A` … `KEY_Z`, `KEY_0` … `KEY_9` |
| Numpad | `KEY_NUMPAD0` … `KEY_NUMPAD9`, `KEY_MULTIPLY`, `KEY_ADD`, `KEY_SUBTRACT`, `KEY_DECIMAL`, `KEY_DIVIDE` |
| Punctuation | `KEY_SEMICOLON`, `KEY_EQUAL`, `KEY_COMMA`, `KEY_MINUS`, `KEY_PERIOD`, `KEY_SLASH`, `KEY_GRAVE`, `KEY_LEFT_BRACKET`, `KEY_BACKSLASH`, `KEY_RIGHT_BRACKET`, `KEY_APOSTROPHE` |

| Modifier | Value |
| --- | --- |
| `MOD_SHIFT` | `1` |
| `MOD_CONTROL` | `2` |
| `MOD_ALT` | `4` |
| `MOD_SUPER` | `8` |
| `MOD_CAPS_LOCK` | `16` |
| `MOD_NUM_LOCK` | `32` |

| Button | Value |
| --- | --- |
| `MOUSE_LEFT` | `0` |
| `MOUSE_RIGHT` | `1` |
| `MOUSE_MIDDLE` | `2` |
| `MOUSE_X1`, `MOUSE_X2` | `3`, `4` — the side buttons |

```sere
import gl

def main() -> i32:
    window: gl.Window = gl.Window("input", 640, 360)
    if not window.is_open():
        return 1
    window.make_current()

    while window.is_open():
        window.poll()
        if window.should_close() or window.key_pressed(gl.KEY_ESCAPE):
            window.destroy()

        # Held: continuous action, such as camera movement.
        if window.key(gl.KEY_W):
            print("forward")

        # Pressed: one-shot action, such as a jump or a toggle.
        if window.key_pressed(gl.KEY_SPACE):
            print("jump")

        # Modifiers combine, so test each bit separately.
        if window.modifier(gl.MOD_CONTROL) and window.key_pressed(gl.KEY_S):
            print("save")

        if window.mouse_pressed(gl.MOUSE_LEFT):
            print("click at", window.mouse_x(), window.mouse_y())

        typed: i32 = window.typed_char()
        if typed != 0:
            print("char code", typed)

        window.swap()

    return 0
```

## Timing

| Method | Result |
| --- | --- |
| `time()` | Seconds since the window was created, monotonic |
| `dt()` | Seconds since the previous frame |

Both are monotonic `f64` values, so they are safe for simulation and never jump
backwards when the clock is adjusted. `dt()` is measured over the work of the
previous frame, which makes it the right multiplier for anything that should
move at a constant speed:

```sere
import gl

def main() -> i32:
    window: gl.Window = gl.Window("motion", 640, 360)
    if not window.is_open():
        return 1
    window.make_current()

    speed: f64 = 0.6            # units per second
    x: f64 = 0.0

    while window.is_open():
        window.poll()
        if window.should_close():
            window.destroy()

        delta: f64 = window.dt()
        if delta > 0.1:
            delta = 0.1         # clamp after a stall so nothing teleports
        x = x + speed * delta
        if x > 0.8:
            x = -0.8

        gl.viewport(0, 0, window.framebuffer_width(), window.framebuffer_height())
        gl.clear(gl.GL_COLOR_BUFFER_BIT)
        gl.rect(x, -0.1, 0.2, 0.2)
        window.swap()

    return 0
```

## Pipeline state

State lives in `gl.state`. These are the `glEnable`-style switches and the
per-fragment rules that are not tied to an object.

### Clearing

| Function | Result |
| --- | --- |
| `clear_color(r, g, b, a)` | Colour written by `clear(GL_COLOR_BUFFER_BIT)` |
| `clear_depth(depth)` | Value written by `clear(GL_DEPTH_BUFFER_BIT)`, normally `1.0` |
| `clear_stencil(value)` | Value written by `clear(GL_STENCIL_BUFFER_BIT)` |
| `clear(mask)` | Clear the selected buffers; `mask` is a sum of the three `*_BUFFER_BIT` values |

```sere
import gl

def main() -> i32:
    window: gl.Window = gl.Window("clear", 320, 200)
    if not window.is_open():
        return 1
    window.make_current()

    gl.clear_color(0.05, 0.06, 0.09, 1.0)
    gl.clear_depth(1.0)
    gl.clear_stencil(0)
    mask: u32 = gl.GL_COLOR_BUFFER_BIT + gl.GL_DEPTH_BUFFER_BIT + gl.GL_STENCIL_BUFFER_BIT
    gl.clear(mask)

    window.swap()
    window.destroy()
    return 0
```

| Clear bit | Value |
| --- | --- |
| `GL_COLOR_BUFFER_BIT` | `0x4000` |
| `GL_DEPTH_BUFFER_BIT` | `0x0100` |
| `GL_STENCIL_BUFFER_BIT` | `0x0400` |

### Viewport and scissor

| Function | Result |
| --- | --- |
| `viewport(x, y, width, height)` | Map normalised device coordinates onto the surface; `y` counts up from the bottom |
| `viewport_whole(width, height)` | `viewport(0, 0, width, height)` |
| `scissor(x, y, width, height)` | The rectangle kept while `GL_SCISSOR_TEST` is enabled |

Call `viewport` once per frame with the framebuffer size. Forgetting it after a
resize — or after rendering off-screen — is the usual cause of a squashed or
offset image.

### Capabilities

| Function | Result |
| --- | --- |
| `enable(cap)`, `disable(cap)` | Turn a capability on or off |
| `is_enabled(cap)` | Query one |
| `set_enabled(cap, on)` | Set one from a `bool` |

Frequently toggled capabilities:

| Capability | Value | Turns on |
| --- | --- | --- |
| `GL_BLEND` | `0x0BE2` | Colour blending |
| `GL_DEPTH_TEST` | `0x0B71` | Depth comparison |
| `GL_CULL_FACE` | `0x0B44` | Discarding back faces |
| `GL_SCISSOR_TEST` | `0x0C11` | Scissor rectangle |
| `GL_STENCIL_TEST` | `0x0B90` | Stencil test |
| `GL_LINE_SMOOTH` | `0x0B20` | Antialiased lines |
| `GL_POLYGON_OFFSET_FILL` | `0x8037` | Depth bias for filled polygons |
| `GL_MULTISAMPLE` | `0x809D` | Multisample rasterisation |

### Blending

| Function | Result |
| --- | --- |
| `blend_func(source, destination)` | Source and destination factors |
| `blend_func_separate(src_rgb, dst_rgb, src_alpha, dst_alpha)` | Per-channel factors |
| `blend_equation(mode)` | `GL_FUNC_ADD`, `GL_FUNC_SUBTRACT`, `GL_FUNC_REVERSE_SUBTRACT`, `GL_MIN`, `GL_MAX` |
| `blend_equation_separate(rgb, alpha)` | Colour and alpha modes separately |
| `blend_color(r, g, b, a)` | The constant colour used by `GL_CONSTANT_COLOR` and friends |
| `alpha_blend()` | `enable(GL_BLEND)` plus `GL_SRC_ALPHA`/`GL_ONE_MINUS_SRC_ALPHA` |
| `additive_blend()` | `enable(GL_BLEND)` plus `GL_SRC_ALPHA`/`GL_ONE`, for glows and particles |

| Factor | Value |
| --- | --- |
| `GL_ZERO`, `GL_ONE` | `0`, `1` |
| `GL_SRC_COLOR`, `GL_DST_COLOR` | `0x0300`, `0x0306` |
| `GL_SRC_ALPHA`, `GL_ONE_MINUS_SRC_ALPHA` | `0x0302`, `0x0303` |
| `GL_SRC_ALPHA_SATURATE` | `0x0308` |
| `GL_CONSTANT_ALPHA` | `0x8003` |

### Depth and stencil

| Function | Result |
| --- | --- |
| `depth_func(func)` | Comparison used by the depth test |
| `depth_mask(write)` | `1` writes depth, `0` tests without writing — the shadow-map idiom |
| `depth_range(near_z, far_z)` | Map the depth buffer's `0…1` onto a value range |
| `stencil_func(func, reference, mask)` | How the stencil test compares |
| `stencil_op(fail, depth_fail, depth_pass)` | What happens on each outcome |
| `stencil_mask(mask)` | Bits that may be written |
| `default_depth_state()` | `enable(GL_DEPTH_TEST)`, `depth_func(GL_LESS)`, `depth_mask(1)` |

| Comparison | Value |
| --- | --- |
| `GL_NEVER`, `GL_LESS`, `GL_EQUAL`, `GL_LEQUAL`, `GL_GREATER`, `GL_ALWAYS` | `0x0200`, `0x0201`, `0x0202`, `0x0203`, `0x0204`, `0x0207` |

| Stencil action | Value |
| --- | --- |
| `GL_KEEP`, `GL_ZERO`, `GL_REPLACE`, `GL_INCR`, `GL_DECR` | `0x1E00`, `0`, `0x1E01`, `0x1E02`, `0x1E03` |

### Rasterisation

| Function | Result |
| --- | --- |
| `color_mask(r, g, b, a)` | Which channels may be written; `1` or `0` each |
| `cull_face(mode)` | `GL_FRONT`, `GL_BACK`, or `GL_FRONT_AND_BACK` |
| `front_face(mode)` | Which winding is front-facing: `GL_CCW` (`0x0901`) or `GL_CW` (`0x0900`) |
| `polygon_mode(face, mode)` | `GL_FILL` (`0x1B02`), `GL_LINE` (`0x1B01`), or `GL_POINT` |
| `wireframe(on)` | Switch between filled and outlined triangles |
| `polygon_offset(factor, units)` | Depth bias, to stop coplanar surfaces from z-fighting |
| `line_width(width)`, `point_size(size)` | Rasterised line and point size in pixels |
| `logic_op(op)` | Bitwise colour operation while `GL_COLOR_LOGIC_OP` is enabled |

### Pixel store, hints, and output

| Function | Result |
| --- | --- |
| `pixel_store(pname, value)` | `GL_UNPACK_ALIGNMENT` (`0x0CF5`), `GL_PACK_ALIGNMENT` (`0x0D05`), `GL_UNPACK_ROW_LENGTH`, … |
| `hint(target, mode)` | `GL_PERSPECTIVE_CORRECTION_HINT`, `GL_LINE_SMOOTH_HINT`, … with `GL_DONT_CARE`, `GL_FASTEST`, `GL_NICEST` |
| `draw_buffer(buffer)` | Colour attachment written by fragment output: `GL_BACK`, `GL_COLOR_ATTACHMENT0`, … |
| `read_buffer(buffer)` | Colour attachment read by `read_pixels` and pixel copies |
| `sample_coverage(value, invert)` | Fraction of samples kept, optionally inverted |
| `finish()` | Wait until every issued command has completed |
| `flush()` | Start executing queued commands without waiting |

`finish()` is what you want before reading pixels back for a screenshot;
`flush()` is the cheap version that only starts the work.

## Immediate-mode drawing

`gl.immediate` is the fixed-function path: geometry sent between `begin(mode)`
and `end()`. Every vertex is built from the state set since the last one, so
colour or texture coordinate comes first, then the position.

```sere
import gl

def main() -> i32:
    window: gl.Window = gl.Window("immediate", 480, 320)
    if not window.is_open():
        return 1
    window.make_current()

    while window.is_open():
        window.poll()
        if window.should_close() or window.key_pressed(gl.KEY_ESCAPE):
            window.destroy()

        gl.viewport(0, 0, window.framebuffer_width(), window.framebuffer_height())
        gl.clear_color(0.1, 0.1, 0.14, 1.0)
        gl.clear(gl.GL_COLOR_BUFFER_BIT)

        # A quad, a circle, a line, and a triangle, using the helpers.
        gl.color3f(0.9, 0.4, 0.3)
        gl.rect(-0.9, -0.9, 0.5, 0.5)

        gl.color3f(0.3, 0.8, 0.9)
        gl.circle(0.4, 0.4, 0.35, 64)

        gl.color3f(1.0, 1.0, 1.0)
        gl.line(-0.9, 0.9, 0.9, 0.2)

        gl.triangle(0.0, 0.9, -0.3, 0.4, 0.3, 0.4)

        window.swap()

    return 0
```

| Function | Result |
| --- | --- |
| `begin(mode)`, `end()` | Open and close a primitive batch |
| `vertex2f(x, y)`, `vertex3f(x, y, z)` | Add a vertex; sugar for `vertex2f` is `vertex(x, y)` |
| `color3f(r, g, b)`, `color4f(r, g, b, a)` | Colour for the following vertices; sugar is `color` |
| `texcoord2f(u, v)` | Texture coordinate of the next vertex |
| `normal3f(x, y, z)` | Normal for the built-in lighting models |
| `rect(x, y, width, height)` | Filled rectangle as `GL_QUADS` |
| `line(x0, y0, x1, y1)` | Segment as `GL_LINES` |
| `triangle(x0, y0, x1, y1, x2, y2)` | Filled triangle as `GL_TRIANGLES` |
| `circle(x, y, radius, segments = 48)` | Filled circle as a `GL_TRIANGLE_FAN` |

| Primitive mode | Value | Use |
| --- | --- | --- |
| `GL_POINTS` | `0` | Individual points |
| `GL_LINES` | `1` | Pairs of vertices |
| `GL_LINE_LOOP` | `2` | Closed polyline |
| `GL_LINE_STRIP` | `3` | Open polyline |
| `GL_TRIANGLES` | `4` | Triples of vertices |
| `GL_TRIANGLE_STRIP` | `5` | Shared-edge triangle strip |
| `GL_TRIANGLE_FAN` | `6` | Fan, for discs and cones |
| `GL_QUADS` | `7` | Quadruples — compatibility profiles only |
| `GL_QUAD_STRIP` | `8` | Quad strip |
| `GL_POLYGON` | `9` | One convex polygon |

Immediate mode is only guaranteed in a compatibility profile. The runtime's WGL
context is one, which is exactly why this path works here and would not in a core
profile. `gl.immediate` is the right tool for a debug overlay, a chart, a
prototype, or a simple game; use [buffers](#buffers-and-vertex-arrays) for
anything that draws the same mesh every frame.

## Shaders

`gl.shaders` covers GLSL compilation and linking. Compilation is per stage;
linking joins a vertex and a fragment stage into a program. Both steps report
failure through a log rather than a return code, and the classes keep that text
on the object.

| `Shader` | Result |
| --- | --- |
| `Shader(kind, source)` | Compile `source` as `GL_VERTEX_SHADER` or `GL_FRAGMENT_SHADER` |
| `vertex_shader(source)`, `fragment_shader(source)` | The same, returning a `Shader` |
| `.id` | The driver name, useful for rendering to a texture |
| `.kind` | Stage enum it was created with |
| `.compiled` | `True` when compilation succeeded |
| `.log` | Compiler diagnostics; empty when it compiled |
| `.ok()` | `compiled` as a predicate |
| `.destroy()` | Delete the driver object |

| `Program` | Result |
| --- | --- |
| `Program()` | Create an empty program |
| `.attach(shader)`, `.detach(shader)` | Add or remove a stage |
| `.bind_attribute(index, name)` | Fix a vertex attribute location; call before `link()` |
| `.link()` | Link the attached stages; `False` on failure |
| `.ok()`, `.linked`, `.log` | Link state and the linker log |
| `.use()` | Install the program for subsequent draws |
| `.validate()` | Ask the driver whether it can run in the current state |
| `.uniform(name)`, `.attribute(name)` | Locations, or `-1` when inactive |
| `.set_float(name, x)` | One float uniform |
| `.set_vec2/3/4(name, …)` | Vector uniforms |
| `.set_int(name, value)`, `.set_sampler(name, unit)` | Integer and sampler uniforms |
| `.set_mat3(name, values)`, `.set_mat4(name, values)` | Matrices from `list[f64]` |
| `.set_vec_at(location, values)` | Vector at a cached location |
| `.destroy()` | Delete the driver object |
| `build(vertex, fragment)` | Compile both and link, returning the `Program` |

```sere
import gl

VERTEX: str = """#version 120
attribute vec2 position;
attribute vec3 colour;
varying vec3 tint;
void main() {
    tint = colour;
    gl_Position = vec4(position, 0.0, 1.0);
}
"""

FRAGMENT: str = """#version 120
varying vec3 tint;
void main() {
    gl_FragColor = vec4(tint, 1.0);
}
"""

def main() -> i32:
    window: gl.Window = gl.Window("shaders", 480, 320)
    if not window.is_open():
        return 1
    window.make_current()

    program: gl.Program = gl.build(VERTEX, FRAGMENT)
    if not program.ok():
        print("shader log:", program.log)
        window.destroy()
        return 1

    while window.is_open():
        window.poll()
        if window.should_close() or window.key_pressed(gl.KEY_ESCAPE):
            window.destroy()

        gl.viewport(0, 0, window.framebuffer_width(), window.framebuffer_height())
        gl.clear(gl.GL_COLOR_BUFFER_BIT)
        program.use()
        program.set_float("time", window.time())
        window.swap()

    program.destroy()
    return 0
```

### Uniforms

- **Scalars arrive as `f64` and are narrowed to `float`.** `set_vec4` takes four
  numbers because a `vec4` is four floats.
- **Matrices are `list[f64]` of 9 or 16 values, stored column-major** at the
  driver boundary: `glUniformMatrix4fv` is handed `transpose = 0`, so the block
  is read column by column. The `matrix` module stores **row-major**, which is the
  same transform in the opposite order, so a `matrix.Mat4` must be transposed on
  the way in — `gl.set_matrix4(program, name, value)` and
  `gl.matrix4_data(value)` do that for you. `program.set_mat4(name, values)`
  takes an already-column-major block. See
  [Matrices and 3D math](#matrices-and-3d-math).
- **Samplers take a texture unit index**, not a texture name: bind the texture to
  `GL_TEXTURE0 + n`, then `program.set_sampler("image", n)`.
- **A misspelled or unused uniform is harmless.** `uniform("nope")` returns `-1`
  and the driver ignores a uniform set at location `-1`, so a setter on a name
  the compiler optimised away is a no-op rather than an error.
- **Cache the location** with `location: i32 = program.uniform("mvp")` and use
  `set_vec_at`/`uniform_mat4` in a loop; the by-name setters look the location up
  every call, which is fine for a handful of uniforms and wasteful for thousands.
- **`#version 120` is the safe dialect** here: the context is a 2.1 compatibility
  profile, so `attribute`/`varying`/`gl_FragColor` are correct, and the driver
  (typically 4.6) accepts it. A `#version 330 core` shader would need a core
  context, which this binding does not create.

```sere
import gl

VERTEX: str = """#version 120
attribute vec2 position;
uniform mat4 transform;
void main() {
    gl_Position = transform * vec4(position, 0.0, 1.0);
}
"""

FRAGMENT: str = """#version 120
uniform vec4 tint;
void main() {
    gl_FragColor = tint;
}
"""

def main() -> i32:
    window: gl.Window = gl.Window("uniforms", 480, 320)
    if not window.is_open():
        return 1
    window.make_current()

    program: gl.Program = gl.build(VERTEX, FRAGMENT)
    if not program.ok():
        print(program.log)
        window.destroy()
        return 1
    program.use()

    # Column-major 4x4: three scaled basis axes and no translation. This scalar
    # matrix is symmetric, so the storage order does not change it; for a
    # rotation or a translation use gl.set_matrix4, which transposes.
    transform: list[f64] = [
        2.0, 0.0, 0.0, 0.0,
        0.0, 2.0, 0.0, 0.0,
        0.0, 0.0, 2.0, 0.0,
        0.0, 0.0, 0.0, 1.0]
    program.set_mat4("transform", transform)
    program.set_vec4("tint", 1.0, 0.4, 0.2, 1.0)
    print(program.uniform("nonexistent"))

    window.swap()
    program.destroy()
    window.destroy()
    return 0
```

## Matrices and 3D math

`gl.transform` is the math layer: projections, view matrices, a camera, a
composable transform, the uploads that feed them to a shader, and the
screen-space helpers that tell you where a world point ended up. It is pure Sere
on top of `matrix`, `vec`, and the uniform setters in `gl.shaders` — no new
natives, nothing to free, and no context needed to compute a matrix.

| Module | Contents |
| --- | --- |
| `gl.transform` | `perspective`, `perspective_deg`, `perspective_window`, `ortho`, `ortho_2d`, `ortho_centered`, `look_at`, `Camera`, `Transform`, `compose`, `set_matrix4`, `set_matrix3`, `set_normal_matrix`, `project_to_screen`, `ray_from_screen` |
| `matrix` | The raw math: `Mat2`/`Mat3`/`Mat4`, `translate`, `rotate_x/y/z`, `rotate_axis`, `scale`, `perspective`, `ortho`, `look_at`, `inverse`, `upper3` |
| `vec` | `Vec2`/`Vec3`/`Vec4` with `add`, `sub`, `scale`, `dot`, `cross`, `normalized`, `to_list` |

### Two storage orders, one transform

This is the single most important thing to get right, and the one that silently
produces a wrong picture rather than an error.

| | `matrix` | An OpenGL `mat4` uniform |
| --- | --- | --- |
| Storage | **Row-major**: `data[0..3]` is row 0 | **Column-major**: a block read column by column |
| Convention | Column vector: `p' = M * p` | Column vector: `gl_Position = mvp * vec4(p, 1.0)` |
| Translation lives in | `data[3]`, `data[7]`, `data[11]` | Block indices 12, 13, 14 |

They describe the same transform in opposite orders, so the block you upload has
to be the transpose of `matrix`'s data — and the runtime hands the driver
`transpose = 0`, so there is no shortcut on the driver side.

```sere ignore
program.set_mat4("model", model.data)   # wrong: the shader sees the transpose
gl.set_matrix4(program, "model", model) # right: transposes on the way in

# or, when you want the block yourself:
block: list[f64] = gl.matrix4_data(model)
program.set_mat4("model", block)
```

For a translation this is unmistakable: `matrix.translate(1.0, 0.0, 0.0).data`
puts the 1 at index 3, while `gl.matrix4_data(...)` puts it at index 12, which is
where a column-major reader expects the fourth column.

The practical rule: **a `matrix.Mat4` never goes to `program.set_mat4` directly.**
Use `gl.set_matrix4`, and keep the raw path for hand-written blocks.

### Projections

```sere
import gl
import matrix

def main() -> i32:
    aspect: f64 = gl.aspect_ratio(1280, 720)

    # Degrees for people, radians for the matrix module.
    perspective: matrix.Mat4 = gl.perspective_deg(60.0, aspect, 0.1, 1000.0)
    same: matrix.Mat4 = gl.perspective(gl.to_radians(60.0), aspect, 0.1, 1000.0)

    # Two dimensions, in pixels: the origin is the top-left corner and y grows
    # downward, which is what a sprite or UI layer wants.
    screen: matrix.Mat4 = gl.ortho_2d(1280, 720)

    # Y up, centred on the origin: the shape a 2D game or a plot wants.
    centred: matrix.Mat4 = gl.ortho_centered(16.0, 9.0)

    # An explicit box, for a CAD view or a shadow map pass.
    box: matrix.Mat4 = gl.ortho(-10.0, 10.0, -5.0, 5.0, 0.1, 100.0)

    print(perspective.data[0], same.data[5], screen.data[0], centred.data[5], box.data[0])
    return 0
```

| Function | Use |
| --- | --- |
| `perspective_deg(fov_degrees, aspect, near_z, far_z)` | The usual projection; 60 degrees is a good default |
| `perspective(fov_radians, aspect, near_z, far_z)` | The same thing in radians, for math you already have |
| `perspective_window(window, fov_degrees?, near?, far?)` | Recomputes the aspect from the framebuffer each call |
| `aspect_ratio(width, height)` | `width / height`, and `1.0` when the height is zero |
| `aspect_of_window(window)` | The same, from a window's framebuffer |
| `ortho_2d(width, height)` | Pixels, y down, origin top-left |
| `ortho_centered(width, height)` | Y up, centred on the origin |
| `ortho(left, right, bottom, top, near, far)` | An explicit box |

A minimised window reports a zero-sized framebuffer. `aspect_ratio` returns `1.0`
there instead of dividing by zero, so one frame of a degenerate projection is not
something you have to guard for.

### Views and the camera

A view matrix is just another matrix, but `Camera` keeps the five numbers that
describe one together: a position, a target, an up vector, a field of view, and a
depth range. It owns its aspect ratio rather than reading a window, so the same
camera works for an off-screen target.

| `Camera` | Result |
| --- | --- |
| `Camera(position, target)` | Create one; the defaults are 60 degrees, `(0, 1, 0)` up, 0.1 to 1000 |
| `.view()`, `.projection()`, `.view_projection()` | The three matrices |
| `.set_aspect(w, h)`, `.set_aspect_ratio(f)`, `.set_fov_degrees(f)`, `.set_clip(near, far)` | The lens |
| `.orbit(yaw_degrees, pitch_degrees, distance)` | Place it on a sphere around its target |
| `.move(dx, dy, dz)`, `.dolly(distance)` | Slide it, or move along its view direction |
| `.forward()`, `.right()`, `.true_up()` | Unit basis vectors |
| `.upload(program, view_name?, projection_name?)` | Two `mat4` uniforms, named `view` and `projection` by default |
| `.upload_mvp(program, name, model)` | One combined `mat4` |
| `.ray(x, y, width, height)` | A picking ray through a pixel |

```sere
import gl
import matrix

def main() -> i32:
    # Look at a point from an angle, rather than picking the position by hand.
    camera: gl.Camera = gl.orbit_camera(gl.vec3(0.0, 1.0, 0.0), 35.0, 20.0, 8.0)
    camera.set_fov_degrees(50.0)
    camera.set_clip(0.1, 200.0)
    camera.set_aspect_ratio(16.0 / 9.0)

    print(camera.position.to_list())
    print(camera.forward().to_list(), camera.right().to_list())

    view: matrix.Mat4 = camera.view()
    projection: matrix.Mat4 = camera.projection()
    combined: matrix.Mat4 = camera.view_projection()
    print(view.data[11], projection.data[0], combined.data[0])
    return 0
```

Orbiting is **absolute**, not incremental: `orbit(35.0, 20.0, 8.0)` always puts the
camera in the same place, so it is safe to call every frame from mouse input
without drift accumulating.

The other view constructors are for when you already know two points:

| Function | Use |
| --- | --- |
| `look_at(eye, target, up)` | From three `Vec3` values |
| `look_at_y_up(eye, target)` | The common case, with the world's y axis up |
| `look_at_dir(eye, forward, up)` | From a position and a direction |
| `look_at_values(eye, target, up)` | From three plain three-element lists |

### Transforms

`Transform` is position, Euler rotation, and scale, composed into a model matrix.
The order is fixed and deliberate: `translate * rotate_z * rotate_y * rotate_x *
scale`, so a model is scaled first, then rotated about x, then y, then z, and
finally moved.

```sere
import gl

def main() -> i32:
    model: gl.Transform = gl.Transform()
    model.position = gl.vec3(2.0, 0.0, 0.0)
    model.rotation = gl.vec3(0.0, 0.0, gl.to_radians(30.0))
    model.scaling = gl.vec3(2.0, 1.0, 1.0)

    # The model matrix, and the normal matrix that goes with an uneven scale.
    print(model.matrix().data[3], model.normal_matrix().data[0])

    # The same thing with the mutating helpers: turn spins by a frame's worth
    # of radians, move slides it along, scale_by multiplies.
    model.turn(0.0, 0.01, 0.0)
    model.move(0.0, 0.0, -0.5)
    model.scale_by(1.0, 1.0, 1.0)
    print(model.position.to_list())

    # A unit quad placed at a pixel rectangle, for ortho_2d drawing.
    quad: gl.Transform = gl.Transform()
    print(gl.sprite_matrix(10.0, 20.0, 30.0, 40.0).transform_point(1.0, 1.0, 0.0))
    return 0
```

When the matrix has to be built from something other than position/rotation/scale
— a bone, a fitted basis, a camera-relative frame — use `compose`, which
multiplies a list left to right so the last entry applies to the point first:

| Function | Result |
| --- | --- |
| `compose([a, b, c])` | `a * b * c`: c applies first, then b, then a |
| `model_view(model, view)` | The view with the model folded in |
| `model_view_projection(model, view, projection)` | The usual single `mvp` matrix |
| `transform(x, y, z)` | A `Transform` already positioned |
| `sprite_matrix(x, y, width, height)` | A unit quad scaled and placed at a pixel rectangle |

### Uploading matrices

These are the uploads that transpose for you. Every one of them is a no-op when
the uniform name is inactive — a name the GLSL compiler optimised away resolves
to location `-1`, and the driver ignores a uniform set at `-1` — so a misspelled
name quietly does nothing rather than failing.

| Function | Result |
| --- | --- |
| `set_matrix4(program, name, mat4)` | A `mat4` uniform from a `matrix.Mat4` |
| `set_matrix3(program, name, mat3)` | A `mat3` uniform from a `matrix.Mat3` |
| `set_normal_matrix(program, name, model)` | The inverse transpose of the model's upper 3x3 |
| `set_vec2/set_vec3/set_vec4(program, name, v)` | A vector uniform from a `Vec2`/`Vec3`/`Vec4` |
| `set_matrix4_at(location, mat4)` | A cached location, skipping the lookup |
| `matrix4_data(mat4)`, `matrix3_data(mat3)` | The transposed block, for `program.set_mat4` |
| `normal_matrix(model)` | The `Mat3` behind `set_normal_matrix` |
| `set_mvp(program, name, model, camera)` | The combined matrix in one call |

A lit mesh, end to end:

```sere
import gl
import matrix

VERTEX: str = """#version 120
attribute vec3 position;
attribute vec3 normal;
uniform mat4 mvp;
uniform mat4 model;
uniform mat3 normal_matrix;
varying vec3 shade;
void main() {
    shade = normalize(normal_matrix * normal);
    gl_Position = mvp * vec4(position, 1.0);
}
"""

FRAGMENT: str = """#version 120
varying vec3 shade;
void main() {
    gl_FragColor = vec4(abs(shade), 1.0);
}
"""

def main() -> i32:
    window: gl.Window = gl.Window("lit", 640, 400)
    if not window.is_open():
        return 1
    window.make_current()

    program: gl.Program = gl.build(VERTEX, FRAGMENT)
    if not program.ok():
        print(program.log)
        window.destroy()
        return 1
    program.bind_attribute(0, "position")
    program.bind_attribute(1, "normal")
    program.link()
    program.use()

    camera: gl.Camera = gl.orbit_camera(gl.vec3(0.0, 0.0, 0.0), 30.0, 20.0, 6.0)
    camera.set_aspect_ratio(gl.aspect_of_window(window))
    camera.upload(program)

    model: gl.Transform = gl.Transform()
    model.uniform_scale(1.5)
    gl.set_matrix4(program, "model", model.matrix())
    gl.set_normal_matrix(program, "normal_matrix", model.matrix())
    camera.upload_mvp(program, "mvp", model.matrix())

    gl.apply_viewport(window)
    gl.clear_color(0.1, 0.1, 0.12, 1.0)
    gl.clear(gl.GL_COLOR_BUFFER_BIT + gl.GL_DEPTH_BUFFER_BIT)
    gl.enable(gl.GL_DEPTH_TEST)
    gl.draw_arrays(gl.GL_TRIANGLES, 0, 36)
    window.swap()

    program.destroy()
    window.destroy()
    return 0
```

The normal matrix matters as soon as the scale is uneven: without it, a squashed
model lights as if it were sheared, which is the classic "why is my shading
wrong" bug.

### Screen space, depth, and picking

Normalised device coordinates are the -1 to 1 cube the rasteriser works in.
Window coordinates are pixels with y growing downward. These helpers convert, and
`project_to_screen` chains them onto a projection so a label or a health bar can
follow a 3D object.

| Function | Result |
| --- | --- |
| `project_ndc(mvp, point)` | A world `Vec3` to `[x, y, z]` in the cube, divide included |
| `project_to_screen(mvp, point, width, height)` | The same, in pixels |
| `unproject(mvp, ndc)` | Back to a world `Vec3` |
| `screen_from_ndc(x, y, w, h)`, `ndc_from_screen(x, y, w, h)` | The two conversions on their own |
| `linear_depth(z_ndc, near, far)` | A depth buffer value as a distance from the camera |
| `ray_from_screen(camera, x, y, w, h)` | A picking ray as `[ox, oy, oz, dx, dy, dz]` |
| `Camera.ray(x, y, w, h)` | The same, from the camera |

```sere
import gl
import math

def ray_sphere(ray: list[f64], cx: f64, cy: f64, cz: f64, radius: f64) -> f64:
    """Return how far along a ray a sphere is hit, or -1.0 for a miss.

    The ray is [ox, oy, oz, dx, dy, dz] with a unit direction, which is exactly
    what Camera.ray returns.
    """
    ox: f64 = ray[0] - cx
    oy: f64 = ray[1] - cy
    oz: f64 = ray[2] - cz
    b: f64 = ox * ray[3] + oy * ray[4] + oz * ray[5]
    c: f64 = ox * ox + oy * oy + oz * oz - radius * radius
    disc: f64 = b * b - c
    if disc < 0.0:
        return -1.0
    return -b - math.sqrt(disc)

def main() -> i32:
    camera: gl.Camera = gl.make_camera(gl.vec3(0.0, 0.0, 5.0), gl.vec3(0.0, 0.0, 0.0))
    camera.set_aspect_ratio(1.0)

    # A click in the middle of the window, aimed at a sphere at the origin.
    ray: list[f64] = camera.ray(320.0, 240.0, 640, 480)
    print("ray:", ray)
    print("hit at:", ray_sphere(ray, 0.0, 0.0, 0.0, 1.0))
    print("misses:", ray_sphere(ray, 3.0, 0.0, 0.0, 1.0))

    # Where a world point lands on screen, for an overlay.
    mvp: matrix.Mat4 = camera.view_projection()
    print(gl.project_to_screen(mvp, gl.vec3(1.0, 1.0, 0.0), 640, 480))
    return 0
```

Depth buffers are non-linear, so almost all of the precision sits close to the
near plane. `linear_depth` recovers a distance from a sampled depth value, which
is what depth-of-field, fog, and soft particles need.

A picking ray starts **on the near plane**, not at the camera, so marching along
it never hits something between the camera and its own near clip.

### Vertex data

`gl.transform` also flattens vectors for upload, so a geometry builder can work in
`Vec3`/`Vec4` and hand the result straight to a buffer.

| Function | Result |
| --- | --- |
| `points_to_data(points)` | A `list[Vec3]` as a flat `list[f64]` for `buffer_data_f64` |
| `vectors4_to_data(values)` | The same for `Vec4`, which suits colours and tangent frames |
| `fan_indices(vertex_count)` | Triangle indices for a fan, for `buffer_data_i32` |
| `quad_vertices(x, y, width, height)` | Two triangles as twelve numbers, wound counter-clockwise |

```sere
import gl
import vec

def main() -> i32:
    ring: list[vec.Vec3] = []
    step: i32 = 0
    while step < 12:
        angle: f64 = gl.to_radians((step as f64) * 30.0)
        ring.append(gl.vec3(angle, 0.0, 0.0))
        step = step + 1

    positions: list[f64] = gl.points_to_data(ring)
    colours: list[f64] = gl.vectors4_to_data([gl.vec4(1.0, 0.0, 0.0, 1.0), gl.vec4(0.0, 1.0, 0.0, 1.0)])
    indices: list[i32] = gl.fan_indices(12)
    quad: list[f64] = gl.quad_vertices(0.0, 0.0, 32.0, 32.0)

    print(len(positions), len(colours), len(indices), len(quad))
    return 0
```

### Window helpers

| Function | Result |
| --- | --- |
| `apply_viewport(window)` | Set the viewport to the whole framebuffer |
| `apply_frame(camera, window)` | The viewport **and** the camera's aspect, in one call |

```sere ignore
while window.is_open():
    window.poll()
    gl.apply_frame(camera, window)   # resize-safe: viewport and aspect together
    ...
```

The framebuffer size changes when a window moves between displays with different
scaling, not only when the user drags an edge, so this belongs in the frame loop
rather than in a resize branch.

### Recipes

A lit, spinning cube — the program most 3D code starts from, and the one that
exercises the whole layer: camera, projection, model transform, normals, depth
buffer, and buffers.

```sere
import gl
import matrix

VERTEX: str = """#version 120
attribute vec3 position;
attribute vec3 normal;
uniform mat4 mvp;
uniform mat3 normal_matrix;
varying vec3 shade;
void main() {
    shade = normalize(normal_matrix * normal);
    gl_Position = mvp * vec4(position, 1.0);
}
"""

FRAGMENT: str = """#version 120
varying vec3 shade;
void main() {
    gl_FragColor = vec4(normalize(shade) * 0.5 + 0.5, 1.0);
}
"""

CUBE: list[f64] = [
    -1.0, -1.0,  1.0,  0.0,  0.0,  1.0,
     1.0, -1.0,  1.0,  0.0,  0.0,  1.0,
     1.0,  1.0,  1.0,  0.0,  0.0,  1.0,
    -1.0, -1.0,  1.0,  0.0,  0.0,  1.0,
     1.0,  1.0,  1.0,  0.0,  0.0,  1.0,
    -1.0,  1.0,  1.0,  0.0,  0.0,  1.0]

def main() -> i32:
    window: gl.Window = gl.Window("cube", 640, 400)
    if not window.is_open():
        return 1
    window.make_current()

    program: gl.Program = gl.build(VERTEX, FRAGMENT)
    if not program.ok():
        print(program.log)
        window.destroy()
        return 1
    program.bind_attribute(0, "position")
    program.bind_attribute(1, "normal")
    program.link()
    program.use()

    vao: gl.VertexArray = gl.VertexArray()
    vao.bind()
    vbo: gl.Buffer = gl.Buffer(gl.GL_ARRAY_BUFFER)
    vbo.bind()
    vbo.data_f64(CUBE, gl.GL_STATIC_DRAW)
    vao.float_attribute(0, 3, 24, 0)
    vao.float_attribute(1, 3, 24, 12)

    camera: gl.Camera = gl.orbit_camera(gl.vec3(0.0, 0.0, 0.0), 30.0, 20.0, 8.0)
    camera.upload(program, "view", "projection")

    model: gl.Transform = gl.Transform()
    gl.enable(gl.GL_DEPTH_TEST)
    gl.default_depth_state()

    while window.is_open():
        window.poll()
        if window.should_close() or window.key_pressed(gl.KEY_ESCAPE):
            window.destroy()

        gl.apply_frame(camera, window)
        gl.clear_color(0.1, 0.1, 0.14, 1.0)
        gl.clear(gl.GL_COLOR_BUFFER_BIT + gl.GL_DEPTH_BUFFER_BIT)

        model.turn(0.4 * window.dt(), 0.7 * window.dt(), 0.0)
        gl.set_matrix4(program, "model", model.matrix())
        gl.set_normal_matrix(program, "normal_matrix", model.matrix())
        camera.upload_mvp(program, "mvp", model.matrix())

        gl.draw_arrays(gl.GL_TRIANGLES, 0, 6)
        window.swap()

    vao.destroy()
    vbo.destroy()
    program.destroy()
    return 0
```

An orbit camera driven by the mouse: yaw and pitch come straight from the
accumulated movement, and because `orbit` is absolute there is no drift.

```sere
import gl

def main() -> i32:
    window: gl.Window = gl.Window("orbit", 800, 500)
    if not window.is_open():
        return 1
    window.make_current()

    camera: gl.Camera = gl.orbit_camera(gl.vec3(0.0, 0.0, 0.0), 35.0, 20.0, 6.0)
    yaw: f64 = 35.0
    pitch: f64 = 20.0
    distance: f64 = 6.0

    while window.is_open():
        window.poll()
        if window.should_close() or window.key_pressed(gl.KEY_ESCAPE):
            window.destroy()

        # Drag to orbit, wheel to zoom. The delta setters are per frame.
        if window.mouse_button(gl.MOUSE_LEFT):
            yaw = yaw + (window.mouse_dx() as f64) * 0.4
            pitch = pitch + (window.mouse_dy() as f64) * 0.4
        distance = distance - (window.wheel() as f64) * 0.5
        if distance < 1.0:
            distance = 1.0
        camera.orbit(yaw, pitch, distance)

        gl.apply_frame(camera, window)
        gl.clear_color(0.1, 0.1, 0.12, 1.0)
        gl.clear(gl.GL_COLOR_BUFFER_BIT)
        window.swap()

    window.destroy()
    return 0
```

A sprite layer drawn over a 3D scene: switch to the pixel projection, drop the
depth test, and place unit quads with `sprite_matrix`.

```sere
import gl
import matrix

def main() -> i32:
    window: gl.Window = gl.Window("ui", 800, 500)
    if not window.is_open():
        return 1
    window.make_current()

    gl.apply_viewport(window)
    screen: matrix.Mat4 = gl.ortho_2d(window.framebuffer_width(), window.framebuffer_height())

    health: gl.Transform = gl.Transform()
    health.position = gl.vec3(20.0, 20.0, 0.0)
    health.scaling = gl.vec3(120.0 * 0.4, 14.0, 1.0)

    gl.disable(gl.GL_DEPTH_TEST)
    print(screen.data[0], health.matrix().transform_point(1.0, 1.0, 0.0))
    window.destroy()
    return 0
```

## Buffers and vertex arrays

A buffer holds bytes on the GPU: vertex positions, colours, indices, matrices, or
anything else a shader reads. A vertex array object remembers which buffer feeds
each attribute, which is what the modern draw calls consume. This is the path to
use for geometry that is drawn more than once.

| `Buffer` | Result |
| --- | --- |
| `Buffer(target = GL_ARRAY_BUFFER)` | Create a buffer and remember its target |
| `.bind()`, `.unbind()` | Bind it, or clear the target |
| `.data_f64(values, usage)`, `.data_i32(values, usage)`, `.data_bytes(values, usage)` | Upload a whole block |
| `.update_f64(offset_bytes, values)` | Overwrite part of it |
| `.id`, `.target` | Driver name and target |
| `.destroy()` | Delete it |

| `VertexArray` | Result |
| --- | --- |
| `VertexArray()` | Create a vertex array object |
| `.bind()`, `.unbind()` | Bind it, or return to the default state |
| `.enable(index)`, `.disable(index)` | Turn an attribute array on or off |
| `.pointer(index, size, value_type, stride, offset)` | Describe an attribute in full |
| `.float_attribute(index, size, stride, offset)` | Enable and describe 32-bit floats |
| `.uint_attribute(index, size, stride, offset)` | Enable and describe unsigned 32-bit integers |
| `.destroy()` | Delete it |

The free functions behind them — `gen_buffer`, `bind_buffer`, `buffer_data_*`,
`buffer_sub_f64`, `gen_vao`, `bind_vao`, `enable_attrib`, `attrib_pointer`,
`float_attribute`, `uint_attribute`, `ubyte_attribute`, `draw_arrays`,
`draw_arrays_all`, `draw_elements` — are exported too, for code that wants to
manage names by hand.

**Sizes are bytes.** `stride` and `offset` count bytes, not elements: a packed
`vec2 position; vec3 colour` uses a stride of `20` (`2 * 4 + 3 * 4`) with the
colour at offset `16`. The vertex count for a draw is an element count.

| Buffer target | Value |
| --- | --- |
| `GL_ARRAY_BUFFER` | `0x8892` |
| `GL_ELEMENT_ARRAY_BUFFER` | `0x8893` |

| Usage hint | Value | Meaning |
| --- | --- | --- |
| `GL_STREAM_DRAW` | `0x88E0` | Rewritten almost every frame |
| `GL_STATIC_DRAW` | `0x88E4` | Uploaded once, drawn many times |
| `GL_DYNAMIC_DRAW` | `0x88E8` | Rewritten occasionally |

| Attribute type | Value |
| --- | --- |
| `GL_BYTE`, `GL_UNSIGNED_BYTE` | `0x1400`, `0x1401` |
| `GL_UNSIGNED_SHORT`, `GL_UNSIGNED_INT` | `0x1403`, `0x1405` |
| `GL_FLOAT` | `0x1406` |

```sere
import gl

VERTEX: str = """#version 120
attribute vec2 position;
attribute vec3 colour;
varying vec3 tint;
void main() {
    tint = colour;
    gl_Position = vec4(position, 0.0, 1.0);
}
"""

FRAGMENT: str = """#version 120
varying vec3 tint;
void main() {
    gl_FragColor = vec4(tint, 1.0);
}
"""

def main() -> i32:
    window: gl.Window = gl.Window("mesh", 640, 360)
    if not window.is_open():
        return 1
    window.make_current()

    # Interleaved: x, y, r, g, b per vertex -> stride 20, colour at offset 16.
    vertices: list[f64] = [
        -0.6, -0.5, 1.0, 0.0, 0.0,
         0.6, -0.5, 0.0, 1.0, 0.0,
         0.0,  0.6, 0.0, 0.0, 1.0]
    indices: list[i32] = [0, 1, 2]

    vao: gl.VertexArray = gl.VertexArray()
    vao.bind()

    vbo: gl.Buffer = gl.Buffer(gl.GL_ARRAY_BUFFER)
    vbo.bind()
    vbo.data_f64(vertices, gl.GL_STATIC_DRAW)
    vao.float_attribute(0, 2, 20, 0)      # position: 2 floats at offset 0
    vao.float_attribute(1, 3, 20, 16)     # colour: 3 floats at offset 16

    ebo: gl.Buffer = gl.Buffer(gl.GL_ELEMENT_ARRAY_BUFFER)
    ebo.bind()
    ebo.data_i32(indices, gl.GL_STATIC_DRAW)

    program: gl.Program = gl.build(VERTEX, FRAGMENT)
    if not program.ok():
        print(program.log)
        window.destroy()
        return 1

    while window.is_open():
        window.poll()
        if window.should_close() or window.key_pressed(gl.KEY_ESCAPE):
            window.destroy()

        gl.viewport(0, 0, window.framebuffer_width(), window.framebuffer_height())
        gl.clear(gl.GL_COLOR_BUFFER_BIT)

        program.use()
        vao.bind()
        gl.draw_elements(gl.GL_TRIANGLES, 3, gl.GL_UNSIGNED_INT, 0)

        window.swap()

    ebo.destroy()
    vbo.destroy()
    vao.destroy()
    program.destroy()
    window.destroy()
    return 0
```

| Draw call | Result |
| --- | --- |
| `draw_arrays(mode, first, count)` | `count` vertices from `first`, no index buffer |
| `draw_arrays_all(mode, count)` | The same from the start of the array |
| `draw_elements(mode, count, value_type, offset)` | `count` indices from the bound element buffer, at a byte offset |

Bind the vertex array before drawing, and keep the element buffer bound while the
vertex array is: the element buffer binding is part of the vertex array object's
state, so binding the VAO again restores it.

Streaming vertex data (particles, text) rewrites the buffer each frame:

```sere
import gl

def main() -> i32:
    window: gl.Window = gl.Window("stream", 480, 320)
    if not window.is_open():
        return 1
    window.make_current()

    points: list[f64] = [0.0, 0.0]
    vbo: gl.Buffer = gl.Buffer(gl.GL_ARRAY_BUFFER)
    vbo.bind()
    vbo.data_f64(points, gl.GL_DYNAMIC_DRAW)
    gl.float_attribute(0, 2, 8, 0)

    gl.point_size(6.0)
    gl.draw_arrays(gl.GL_POINTS, 0, 1)
    vbo.update_f64(0, [0.5, 0.5])
    gl.draw_arrays(gl.GL_POINTS, 0, 1)

    vbo.destroy()
    window.destroy()
    return 0
```

## Textures

A texture is a block of pixels the driver samples during rasterisation. In this
binding the data is a `list[byte]`: for four components, four bytes per pixel per
row, rows bottom-first, which is the order OpenGL expects.

| `Texture` | Result |
| --- | --- |
| `Texture(target = GL_TEXTURE_2D)` | Create a texture and remember its target |
| `.bind()` | Bind it to its target |
| `.image_2d(level, internal_format, width, height, format, value_type, pixels)` | Upload an image |
| `.storage(internal_format, width, height, format)` | Allocate an empty image, for a render target |
| `.sub_image_2d(level, x, y, width, height, format, value_type, pixels)` | Replace a rectangle |
| `.parameter(pname, value)` | Set a filter, wrap mode, or clamp colour |
| `.generate_mipmap()` | Build the mip chain |
| `.smooth()`, `.nearest()` | Linear or nearest-neighbour filtering |
| `.repeat()`, `.clamp()` | Tile or stretch at the edges |
| `.id`, `.target` | Driver name and target |
| `.destroy()` | Delete it |

| Free function | Result |
| --- | --- |
| `gen_texture()`, `delete_texture(t)`, `bind_texture(target, t)` | Names and binding |
| `active_texture(unit)`, `texture_unit(index)`, `bind_unit(unit, t)` | Texture units |
| `tex_image2d(...)`, `tex_storage(...)`, `tex_sub_image2d(...)` | Uploads |
| `tex_param(target, pname, value)` | Parameters |
| `generate_mipmap(target)` | Mip chains |
| `smooth_filter(target)`, `nearest_filter(target)`, `repeat_wrap(target)`, `clamp_wrap(target)` | The four common parameter pairs |
| `rgba_pixels(width, height, r, g, b, a)` | A solid RGBA image, ready to upload |

| Parameter | Value | Common values |
| --- | --- | --- |
| `GL_TEXTURE_MIN_FILTER`, `GL_TEXTURE_MAG_FILTER` | `0x2801`, `0x2800` | `GL_NEAREST` (`0x2600`), `GL_LINEAR` (`0x2601`) |
| `GL_TEXTURE_WRAP_S`, `GL_TEXTURE_WRAP_T` | `0x2802`, `0x2803` | `GL_REPEAT` (`0x2901`), `GL_CLAMP_TO_EDGE` (`0x812F`) |
| `GL_TEXTURE_2D` | `0x0DE1` | The only target this binding uses |

**Uploading a full image.**

```sere
import gl

def main() -> i32:
    window: gl.Window = gl.Window("texture", 480, 320)
    if not window.is_open():
        return 1
    window.make_current()

    # 4x4 checkerboard, four bytes per pixel, bottom row first.
    pixels: list[byte] = []
    row: i32 = 0
    while row < 4:
        column: i32 = 0
        while column < 4:
            light: bool = (row + column) % 2 == 0
            pixels.append(230 if light else 40)
            pixels.append(230 if light else 40)
            pixels.append(230 if light else 60)
            pixels.append(255)
            column = column + 1
        row = row + 1

    texture: gl.Texture = gl.Texture()
    texture.bind()
    texture.nearest()
    texture.repeat()
    # A 4x4 RGBA row is 16 bytes: a multiple of four, so no alignment change is
    # needed here. See the note below for odd widths.
    texture.image_2d(0, gl.GL_RGBA, 4, 4, gl.GL_RGBA, gl.GL_UNSIGNED_BYTE, pixels)
    texture.generate_mipmap()

    gl.enable(gl.GL_TEXTURE_2D)
    gl.bind_unit(1, texture.id)
    print(texture.id, gl.texture_unit(1))

    window.swap()
    texture.destroy()
    window.destroy()
    return 0
```

| Upload argument | Meaning |
| --- | --- |
| `level` | Mip level, `0` for the base image |
| `internal_format` | How the driver stores it: `GL_RGBA` (`0x1908`), `GL_RGBA8` (`0x8058`), `GL_DEPTH_COMPONENT24` (`0x81A6`), … |
| `width`, `height` | Pixels |
| `format` | Layout of the data you are handing over: `GL_RGBA`, `GL_RGB` (`0x1907`), `GL_RED` (`0x1903`), `GL_DEPTH_COMPONENT` (`0x1902`) |
| `value_type` | Element type: `GL_UNSIGNED_BYTE` (`0x1401`), `GL_FLOAT` (`0x1406`), … |
| `pixels` | `list[byte]`, bottom row first |

**Row alignment.** The driver defaults to `GL_UNPACK_ALIGNMENT` of `4`, so a row
whose byte length is not a multiple of four is read wrong. An RGBA image is
always a multiple of four; a `GL_RED` image of width 3 (3 bytes per row) is not.
Fix it once, before the upload:

```sere
import gl

def main() -> i32:
    grey: list[byte] = gl.rgba_pixels(3, 2, 128, 128, 128, 255)
    gl.pixel_store(gl.GL_UNPACK_ALIGNMENT, 1)
    gl.bind_texture(gl.GL_TEXTURE_2D, 0)
    gl.tex_image2d(gl.GL_TEXTURE_2D, 0, gl.GL_RGBA, 3, 2, gl.GL_RGBA, gl.GL_UNSIGNED_BYTE, grey)
    print(len(grey))
    return 0
```

**Sampling a texture in a shader.** Enable the stage, bind on a unit, and point
the sampler at that unit:

```sere
import gl
import math

VERTEX: str = """#version 120
attribute vec2 position;
attribute vec2 texcoord;
varying vec2 uv;
void main() {
    uv = texcoord;
    gl_Position = vec4(position, 0.0, 1.0);
}
"""

FRAGMENT: str = """#version 120
uniform sampler2D image;
varying vec2 uv;
void main() {
    gl_FragColor = texture2D(image, uv);
}
"""

def main() -> i32:
    window: gl.Window = gl.Window("sampler", 320, 200)
    if not window.is_open():
        return 1
    window.make_current()

    program: gl.Program = gl.build(VERTEX, FRAGMENT)
    if not program.ok():
        print(program.log)
        window.destroy()
        return 1

    texture: gl.Texture = gl.Texture()
    texture.bind()
    texture.smooth()
    texture.clamp()
    texture.image_2d(0, gl.GL_RGBA, 2, 2, gl.GL_RGBA, gl.GL_UNSIGNED_BYTE,
                     gl.rgba_pixels(2, 2, 200, 40, 60, 255))

    program.use()
    gl.bind_unit(0, texture.id)      # active_texture(GL_TEXTURE0) + bind
    program.set_sampler("image", 0)  # the sampler reads unit 0

    gl.viewport(0, 0, window.framebuffer_width(), window.framebuffer_height())
    gl.clear(gl.GL_COLOR_BUFFER_BIT)
    gl.float_attribute(0, 2, 16, 0)  # position, for the mesh bound here
    gl.float_attribute(1, 2, 16, 8)  # texcoord
    print(gl.pixel_count(2, 2, 4), math.sqrt(4.0))

    window.swap()
    texture.destroy()
    program.destroy()
    window.destroy()
    return 0
```

`tex_storage` is the alternative when an attachment's contents are produced by
rendering rather than uploaded; see [Framebuffers](#framebuffers).

## Framebuffers

Rendering into a texture instead of the window is how shadows, reflections,
mirrors, and post-processing work. `gl.framebuffer` covers the objects and the
pixel readback.

| `Framebuffer` | Result |
| --- | --- |
| `Framebuffer()` | Create one |
| `.bind()`, `.unbind()` | Make it the read/draw target, or go back to the window |
| `.attach_texture(attachment, texture, level = 0)` | Attach one mip level of a texture |
| `.attach_renderbuffer(attachment, renderbuffer)` | Attach a renderbuffer |
| `.status()` | Completeness code |
| `.complete()` | `True` when the attachments form a usable target |
| `.status_name()` | The code as a readable `GL_*` name |
| `.destroy()` | Delete it |

| `Renderbuffer` | Result |
| --- | --- |
| `Renderbuffer()` | Create one |
| `.bind()` | Make it the storage target |
| `.storage(internal_format, width, height)` | Allocate storage, for example `GL_DEPTH_COMPONENT24` |
| `.destroy()` | Delete it |

| Function | Result |
| --- | --- |
| `gen_framebuffer`, `delete_framebuffer`, `bind_framebuffer(target, fbo)` | Names and binding |
| `framebuffer_texture2d(target, attachment, texture_target, texture, level)` | Attach a texture |
| `framebuffer_renderbuffer(target, attachment, renderbuffer_target, rbo)` | Attach a renderbuffer |
| `check_framebuffer(target)`, `is_complete(status)`, `framebuffer_status_name(status)` | Completeness |
| `gen_renderbuffer`, `delete_renderbuffer`, `bind_renderbuffer` | Renderbuffer names |
| `renderbuffer_storage(target, internal_format, width, height)` | Renderbuffer storage |
| `blit_framebuffer(src_x0, src_y0, src_x1, src_y1, dst_x0, dst_y0, dst_x1, dst_y1, mask, filter)` | Copy between the read and draw framebuffers |
| `read_pixels(x, y, width, height, format, value_type)` | Read raw pixels from the read framebuffer |

A renderbuffer is write-only storage, which is all a depth or stencil buffer
needs. A texture attachment can be sampled afterwards, so it is what a colour
attachment usually is.

| Attachment | Value |
| --- | --- |
| `GL_COLOR_ATTACHMENT0` | `0x8CE0` |
| `GL_DEPTH_ATTACHMENT` | `0x8D00` |
| `GL_STENCIL_ATTACHMENT` | `0x8D20` |
| `GL_DEPTH_STENCIL_ATTACHMENT` | `0x821A` |

| Target | Value |
| --- | --- |
| `GL_FRAMEBUFFER` | `0x8D40` |
| `GL_RENDERBUFFER` | `0x8D41` |
| `GL_FRAMEBUFFER_COMPLETE` | `0x8CD5` |

```sere
import gl

def main() -> i32:
    window: gl.Window = gl.Window("off screen", 640, 360)
    if not window.is_open():
        return 1
    window.make_current()

    width: i32 = 256
    height: i32 = 256

    target: gl.Framebuffer = gl.Framebuffer()
    target.bind()

    colour: gl.Texture = gl.Texture()
    colour.bind()
    colour.storage(gl.GL_RGBA, width, height, gl.GL_RGBA)
    target.attach_texture(gl.GL_COLOR_ATTACHMENT0, colour.id)

    depth: gl.Renderbuffer = gl.Renderbuffer()
    depth.bind()
    depth.storage(gl.GL_DEPTH_COMPONENT24, width, height)
    target.attach_renderbuffer(gl.GL_DEPTH_ATTACHMENT, depth.id)

    if not target.complete():
        print("incomplete:", target.status_name())
        window.destroy()
        return 1

    # Render into the texture.
    gl.viewport(0, 0, width, height)
    gl.clear_color(0.2, 0.3, 0.4, 1.0)
    gl.clear(gl.GL_COLOR_BUFFER_BIT + gl.GL_DEPTH_BUFFER_BIT)
    gl.color3f(1.0, 0.8, 0.2)
    gl.rect(-0.5, -0.5, 1.0, 1.0)

    # Back to the window, and use the result as a texture.
    target.unbind()
    gl.viewport(0, 0, window.framebuffer_width(), window.framebuffer_height())
    gl.clear_color(0.05, 0.05, 0.05, 1.0)
    gl.clear(gl.GL_COLOR_BUFFER_BIT)
    gl.enable(gl.GL_TEXTURE_2D)
    gl.bind_unit(0, colour.id)
    gl.begin(gl.GL_QUADS)
    gl.texcoord2f(0.0, 0.0)
    gl.vertex2f(-0.8, -0.8)
    gl.texcoord2f(1.0, 0.0)
    gl.vertex2f(0.8, -0.8)
    gl.texcoord2f(1.0, 1.0)
    gl.vertex2f(0.8, 0.8)
    gl.texcoord2f(0.0, 1.0)
    gl.vertex2f(-0.8, 0.8)
    gl.end()
    gl.disable(gl.GL_TEXTURE_2D)

    window.swap()
    colour.destroy()
    depth.destroy()
    target.destroy()
    window.destroy()
    return 0
```

**Copying between targets.** `blit_framebuffer` moves a rectangle from the read
framebuffer to the draw framebuffer. Set the read target with
`read_buffer`/`bind_framebuffer`, the draw target with `bind_framebuffer`, then
blit with the buffer mask and `GL_NEAREST` or `GL_LINEAR`:

```sere
import gl

def main() -> i32:
    gl.blit_framebuffer(0, 0, 256, 256, 0, 0, 640, 360,
                        gl.GL_COLOR_BUFFER_BIT, gl.GL_LINEAR)
    return 0
```

## Reading pixels back

| Function | Result |
| --- | --- |
| `read_rgba(x, y, width, height)` | `array[i8]`, four bytes per pixel, bottom row first |
| `read_depth(x, y, width, height)` | `array[i8]`, 32-bit floats as raw bytes |
| `read_pixels(x, y, width, height, format, value_type)` | The general form |
| `pixel_at(pixels, width, x, y, components)` | One pixel as `list[i32]` |
| `pixel_count(width, height, components)` | How many numbers the read produces |

The rectangle is read from the **read framebuffer**, and rows come back
bottom-first, so `y = 0` is the bottom row of the window. `pixel_at` takes the
buffer, the row width it was read with, the coordinates, and the component count,
and returns the channels as `i32` in `0…255` for byte data:

```sere
import gl

def main() -> i32:
    window: gl.Window = gl.Window("readback", 64, 64)
    if not window.is_open():
        return 1
    window.make_current()

    gl.viewport(0, 0, 64, 64)
    gl.clear_color(1.0, 0.0, 0.0, 1.0)
    gl.clear(gl.GL_COLOR_BUFFER_BIT)
    gl.finish()                       # make sure the clear has landed

    shot: array[i8] = gl.read_rgba(0, 0, 4, 4)
    print(len(shot), gl.pixel_count(4, 4, 4))

    pixel: list[i32] = gl.pixel_at(shot, 4, 1, 2, 4)
    print(pixel[0], pixel[1], pixel[2], pixel[3])   # 255 0 0 255

    window.destroy()
    return 0
```

Call `finish()` before reading if you need the current frame's results; without
it you may read a buffer whose commands are still queued.

## Queries

| Function | Result |
| --- | --- |
| `available()` | The runtime has a display layer on this host |
| `get_string(name)` | `GL_VENDOR`, `GL_RENDERER`, `GL_VERSION`, `GL_EXTENSIONS`, `GL_SHADING_LANGUAGE_VERSION`, … |
| `vendor()`, `renderer()`, `version()`, `shading_language_version()` | Those four, named |
| `extensions()` | The extension list as `list[str]` |
| `supports(name)` | Whether one extension is advertised |
| `get_integer(pname)`, `get_float(pname)` | The first component of a numeric query |
| `integers(pname, count)`, `floats(pname, count)` | Several components |
| `max_texture_size()` | Largest texture edge |
| `major_version()`, `minor_version()` | GL 3.0 and later; zero on a 2.1 profile |
| `line_width_range()`, `point_size_range()` | `[minimum, maximum]` |
| `max_viewport_dims()` | `[width, height]` |
| `screen_width()`, `screen_height()`, `screen_size()` | Display size, no context needed |

| Query name | Value |
| --- | --- |
| `GL_VENDOR`, `GL_RENDERER`, `GL_VERSION`, `GL_EXTENSIONS` | `0x1F00`…`0x1F03` |
| `GL_SHADING_LANGUAGE_VERSION` | `0x8B8C` |
| `GL_MAX_TEXTURE_SIZE`, `GL_MAX_TEXTURE_UNITS` | `0x0D33`, `0x84E2` |
| `GL_SMOOTH_LINE_WIDTH_RANGE`, `GL_POINT_SIZE_RANGE`, `GL_MAX_VIEWPORT_DIMS` | `0x0B22`, `0x0B12`, `0x0D3A` |

**Driver strings need a current context.** `vendor()` and friends come from
`glGetString`, which answers `""` until a context is current. Numeric limits have
the same rule: before `make_current()` they report `0`:

```sere
import gl

def main() -> i32:
    # No context yet: strings are empty and limits are zero.
    print(gl.get_string(gl.GL_VERSION) == "", gl.max_texture_size())

    window: gl.Window = gl.Window("query", 320, 200)
    if not window.is_open():
        return 1
    window.make_current()

    print(gl.vendor(), "|", gl.renderer(), "|", gl.version())
    print(len(gl.extensions()), gl.supports("GL_ARB_texture_float"))
    print(gl.max_texture_size(), gl.max_viewport_dims())
    print(gl.line_width_range(), gl.point_size_range())

    window.destroy()
    return 0
```

Range and matrix queries need the multi-component getters, because
`get_float`/`get_integer` return only the first component:

```sere
import gl

def main() -> i32:
    window: gl.Window = gl.Window("limits", 320, 200)
    if not window.is_open():
        return 1
    window.make_current()

    viewport: list[i32] = gl.integers(gl.GL_MAX_VIEWPORT_DIMS, 2)
    print(viewport[0], viewport[1])

    line: list[f64] = gl.floats(gl.GL_SMOOTH_LINE_WIDTH_RANGE, 2)
    print(line[0], line[1])

    window.destroy()
    return 0
```

## Errors

OpenGL reports failures by setting a flag rather than returning one. Errors
queue up, so a single call can leave several behind.

| Function | Result |
| --- | --- |
| `take_error()` | Return one queued code and clear it; `GL_NO_ERROR` when none |
| `error_name(code)` | `GL_*` name for a code, or `0x…` for an unknown one |
| `hex_code(code)` | The digits part of that unknown name |
| `error(operation)` | A readable message for one queued error, clearing it; `""` when clean |
| `require(operation)` | Raise `RuntimeError` when an error is queued |
| `pending()` | Whether anything is queued; the queue is drained, so it is a stable snapshot |
| `clear_all()` | Drain the queue, discarding everything |

| Code | Value | Meaning |
| --- | --- | --- |
| `GL_NO_ERROR` | `0` | Nothing queued |
| `GL_INVALID_ENUM` | `0x0500` | An enumerant is not accepted here |
| `GL_INVALID_VALUE` | `0x0501` | A number is out of range |
| `GL_INVALID_OPERATION` | `0x0502` | The call is illegal in the current state |
| `GL_STACK_OVERFLOW`, `GL_STACK_UNDERFLOW` | `0x0503`, `0x0504` | `begin`/`end` nesting |
| `GL_OUT_OF_MEMORY` | `0x0505` | The driver ran out of memory |
| `GL_INVALID_FRAMEBUFFER_OPERATION` | `0x0506` | Draw or read while the target is incomplete |

```sere
import gl

def main() -> i32:
    window: gl.Window = gl.Window("errors", 320, 200)
    if not window.is_open():
        return 1
    window.make_current()

    gl.clear_color(0.0, 0.0, 0.0, 1.0)
    gl.clear(gl.GL_COLOR_BUFFER_BIT)

    # Non-throwing form: a message, empty when the call was clean.
    message: str = gl.error("setup")
    if len(message) > 0:
        print(message)

    # Throwing form, for invariants you want to enforce.
    try:
        gl.require("setup")
    except RuntimeError as failure:
        print("caught", failure)

    # Drain whatever is left.
    while gl.take_error() != gl.GL_NO_ERROR:
        print("stale error")

    print(gl.error_name(gl.GL_INVALID_ENUM), gl.hex_code(0x1234))
    gl.clear_all()
    window.destroy()
    return 0
```

Because the queue is cumulative, the useful pattern is one `require("step")`
after a group of calls rather than after every single one. If you silence errors
during development with `clear_all()`, remember that a queued
`GL_OUT_OF_MEMORY` is worth acting on rather than dropping.

## Recipes

### Resize handling

The viewport follows the framebuffer size, and the projection follows the aspect
ratio. Setting the viewport every frame makes the first half free:

```sere
import gl

def main() -> i32:
    window: gl.Window = gl.Window("resize", 640, 360)
    if not window.is_open():
        return 1
    window.make_current()

    while window.is_open():
        window.poll()
        if window.should_close():
            window.destroy()

        width: i32 = window.framebuffer_width()
        height: i32 = window.framebuffer_height()
        if height <= 0:
            height = 1                # minimised windows report zero

        gl.viewport(0, 0, width, height)
        if window.resized():
            print("resized to", width, height)

        aspect: f64 = (width as f64) / (height as f64)
        print(aspect)                 # feed this into your projection matrix

        gl.clear(gl.GL_COLOR_BUFFER_BIT)
        window.swap()

    return 0
```

### Fixed timestep

For physics or anything that must not depend on the frame rate, accumulate real
time and step in fixed increments, keeping the leftover for the next frame:

```sere
import gl

def main() -> i32:
    window: gl.Window = gl.Window("simulation", 640, 360, gl.WINDOW_VSYNC)
    if not window.is_open():
        return 1
    window.make_current()

    step: f64 = 1.0 / 60.0
    accumulator: f64 = 0.0
    ticks: i64 = 0

    while window.is_open():
        window.poll()
        if window.should_close():
            window.destroy()

        accumulator = accumulator + window.dt()
        while accumulator >= step:
            ticks = ticks + 1      # one simulation step
            accumulator = accumulator - step

        gl.viewport(0, 0, window.framebuffer_width(), window.framebuffer_height())
        gl.clear(gl.GL_COLOR_BUFFER_BIT)
        window.swap()

    print("simulated", ticks, "steps")
    return 0
```

### Rendering to a texture, then post-processing it

The classic two-pass shape: draw the scene into a colour attachment, then draw a
full-screen quad that samples it through a shader.

```sere
import gl

POST: str = """#version 120
uniform sampler2D scene;
varying vec2 uv;
void main() {
    vec3 colour = texture2D(scene, uv).rgb;
    // A cheap greyscale pass, to show the texture arriving in the shader.
    float grey = (colour.r + colour.g + colour.b) / 3.0;
    gl_FragColor = vec4(grey, grey, grey, 1.0);
}
"""

def main() -> i32:
    window: gl.Window = gl.Window("post", 800, 450)
    if not window.is_open():
        return 1
    window.make_current()

    size: i32 = 512
    scene: gl.Framebuffer = gl.Framebuffer()
    scene.bind()

    image: gl.Texture = gl.Texture()
    image.bind()
    image.storage(gl.GL_RGBA, size, size, gl.GL_RGBA)
    scene.attach_texture(gl.GL_COLOR_ATTACHMENT0, image.id)

    depth: gl.Renderbuffer = gl.Renderbuffer()
    depth.bind()
    depth.storage(gl.GL_DEPTH_COMPONENT24, size, size)
    scene.attach_renderbuffer(gl.GL_DEPTH_ATTACHMENT, depth.id)

    if not scene.complete():
        print(scene.status_name())
        window.destroy()
        return 1

    # Pass one: render the scene into the attachment.
    gl.viewport(0, 0, size, size)
    gl.clear_color(0.1, 0.2, 0.3, 1.0)
    gl.clear(gl.GL_COLOR_BUFFER_BIT + gl.GL_DEPTH_BUFFER_BIT)
    gl.color3f(1.0, 0.7, 0.2)
    gl.triangle(-0.6, -0.6, 0.6, -0.6, 0.0, 0.6)

    # Pass two: back to the window, sample the attachment through the shader.
    scene.unbind()
    image.bind()
    image.smooth()
    image.clamp()
    gl.bind_unit(0, image.id)

    post: gl.Program = gl.build("""#version 120
attribute vec2 position;
attribute vec2 texcoord;
varying vec2 uv;
void main() {
    uv = texcoord;
    gl_Position = vec4(position, 0.0, 1.0);
}
""", POST)
    if not post.ok():
        print(post.log)
    else:
        post.use()
        post.set_sampler("scene", 0)

    gl.viewport(0, 0, window.framebuffer_width(), window.framebuffer_height())
    gl.clear(gl.GL_COLOR_BUFFER_BIT)
    gl.rect(-1.0, -1.0, 2.0, 2.0)    # full-screen quad, one texture unit bound
    window.swap()

    print(gl.pixel_count(size, size, 4), image.id)
    post.destroy()
    image.destroy()
    depth.destroy()
    scene.destroy()
    window.destroy()
    return 0
```

### Taking a screenshot

`read_rgba` gives raw RGBA rows bottom-first; a binary PPM (`P6`) wants three
bytes per pixel and top-down rows, so flip while copying. Write the header with
`fs.File.write` and the pixels with `fs.File.write_bytes`:

```sere
import fs
import gl

def save_ppm(path: str, width: i32, height: i32) -> bool:
    """Write the front buffer to a binary PPM (P6). Returns False on failure."""
    shot: array[i8] = gl.read_rgba(0, 0, width, height)
    needed: i64 = (width as i64) * (height as i64) * 4
    if len(shot) < needed:
        return False

    pixels: list[i8] = []
    row: i32 = height - 1                 # PPM rows run top-down
    while row >= 0:
        column: i32 = 0
        while column < width:
            index: i64 = ((row as i64) * (width as i64) + (column as i64)) * 4
            pixels.append(shot[index])
            pixels.append(shot[index + 1])
            pixels.append(shot[index + 2])
            column = column + 1
        row = row - 1

    with fs.open(path, "wb") as file:
        file.write(f"P6\n{width} {height}\n255\n")
        file.write_bytes(pixels)
    return True

def main() -> i32:
    window: gl.Window = gl.Window("screenshot", 320, 200)
    if not window.is_open():
        return 1
    window.make_current()

    gl.viewport(0, 0, 320, 200)
    gl.clear_color(0.9, 0.4, 0.1, 1.0)
    gl.clear(gl.GL_COLOR_BUFFER_BIT)
    gl.finish()
    window.swap()

    print(save_ppm("screenshot.ppm", 320, 200))
    window.destroy()
    return 0
```

## Common enumerants

`gl.constants` has 520 names, generated from the specification. These are the
groups that come up constantly; `stdlib/gl/constants.sere` has the rest, and the
names keep their `GL_` prefix so they are easy to search for.

| Group | Names and values |
| --- | --- |
| Buffer masks | `GL_COLOR_BUFFER_BIT` `0x4000`, `GL_DEPTH_BUFFER_BIT` `0x0100`, `GL_STENCIL_BUFFER_BIT` `0x0400` |
| Primitives | `GL_POINTS` `0`, `GL_LINES` `1`, `GL_LINE_LOOP` `2`, `GL_LINE_STRIP` `3`, `GL_TRIANGLES` `4`, `GL_TRIANGLE_STRIP` `5`, `GL_TRIANGLE_FAN` `6`, `GL_QUADS` `7` |
| Buffer targets | `GL_ARRAY_BUFFER` `0x8892`, `GL_ELEMENT_ARRAY_BUFFER` `0x8893` |
| Usage hints | `GL_STREAM_DRAW` `0x88E0`, `GL_STATIC_DRAW` `0x88E4`, `GL_DYNAMIC_DRAW` `0x88E8` |
| Element types | `GL_BYTE` `0x1400`, `GL_UNSIGNED_BYTE` `0x1401`, `GL_UNSIGNED_SHORT` `0x1403`, `GL_UNSIGNED_INT` `0x1405`, `GL_FLOAT` `0x1406` |
| Formats | `GL_RED` `0x1903`, `GL_RGB` `0x1907`, `GL_RGBA` `0x1908`, `GL_RGBA8` `0x8058`, `GL_DEPTH_COMPONENT` `0x1902`, `GL_DEPTH_COMPONENT24` `0x81A6`, `GL_DEPTH24_STENCIL8` `0x88F0` |
| Shader stages | `GL_VERTEX_SHADER` `0x8B31`, `GL_FRAGMENT_SHADER` `0x8B30` |
| Comparisons | `GL_NEVER` `0x0200`, `GL_LESS` `0x0201`, `GL_EQUAL` `0x0202`, `GL_LEQUAL` `0x0203`, `GL_GREATER` `0x0204`, `GL_ALWAYS` `0x0207` |
| Blend factors | `GL_ZERO` `0`, `GL_ONE` `1`, `GL_SRC_COLOR` `0x0300`, `GL_SRC_ALPHA` `0x0302`, `GL_ONE_MINUS_SRC_ALPHA` `0x0303`, `GL_DST_COLOR` `0x0306`, `GL_SRC_ALPHA_SATURATE` `0x0308` |
| Blend equations | `GL_FUNC_ADD` `0x8006`, `GL_FUNC_SUBTRACT` `0x800A`, `GL_FUNC_REVERSE_SUBTRACT` `0x800B` |
| Capabilities | `GL_BLEND` `0x0BE2`, `GL_DEPTH_TEST` `0x0B71`, `GL_CULL_FACE` `0x0B44`, `GL_SCISSOR_TEST` `0x0C11`, `GL_STENCIL_TEST` `0x0B90`, `GL_LINE_SMOOTH` `0x0B20`, `GL_MULTISAMPLE` `0x809D` |
| Texture targets and parameters | `GL_TEXTURE_2D` `0x0DE1`, `GL_TEXTURE_MIN_FILTER` `0x2801`, `GL_TEXTURE_MAG_FILTER` `0x2800`, `GL_TEXTURE_WRAP_S` `0x2802`, `GL_TEXTURE_WRAP_T` `0x2803` |
| Filter and wrap values | `GL_NEAREST` `0x2600`, `GL_LINEAR` `0x2601`, `GL_REPEAT` `0x2901`, `GL_CLAMP_TO_EDGE` `0x812F` |
| Texture units | `GL_TEXTURE0` `0x84C0` upwards |
| Framebuffers | `GL_FRAMEBUFFER` `0x8D40`, `GL_RENDERBUFFER` `0x8D41`, `GL_COLOR_ATTACHMENT0` `0x8CE0`, `GL_DEPTH_ATTACHMENT` `0x8D00`, `GL_FRAMEBUFFER_COMPLETE` `0x8CD5` |
| Errors | `GL_NO_ERROR` `0`, `GL_INVALID_ENUM` `0x0500`, `GL_INVALID_VALUE` `0x0501`, `GL_INVALID_OPERATION` `0x0502`, `GL_OUT_OF_MEMORY` `0x0505` |

## Gotchas

| Trap | What actually happens | What to do |
| --- | --- | --- |
| Calling GL before `make_current()` | Everything is a silent no-op: object names are `0`, string queries are `""`, limits are `0` | Make the context current right after creating the window |
| Reading pixels without `finish()` | You may read a buffer whose draw commands are still queued | `gl.finish()` before `read_*` |
| Assuming `read_pixels` is top-down | Rows come back bottom-first, like everything else in GL | Index rows as `height - 1 - y`, or flip while copying (see the screenshot recipe) |
| An odd-width row in a byte-typed upload | The default `GL_UNPACK_ALIGNMENT` of `4` misreads rows that are not a multiple of four bytes | `gl.pixel_store(gl.GL_UNPACK_ALIGNMENT, 1)` before the upload |
| Passing element counts as strides | Strides and offsets are **bytes** | Multiply by the element size: `vec3` floats are `12` bytes |
| Expecting a core profile | The runtime creates a WGL compatibility context; `#version 330 core` will not compile | Use `#version 120` and `attribute`/`varying`/`gl_FragColor` |
| A `#version` line that is not first | GLSL requires it to be the very first line of the source string | Build the source with the version line in front |
| Forgetting `viewport` after a resize or an off-screen pass | The image is squashed or offset | Set the viewport every frame from `framebuffer_width()`/`framebuffer_height()` |
| Assuming `available()` means "GPU present" | On Windows it is `True` as soon as the runtime is built with the display layer; the generic GL 1.1 implementation always loads | Branch on `window.is_open()`, and read `gl.version()` for capability |
| Expecting exceptions from a failing GL call | Errors queue silently and the call returns normally | `gl.require("step")` for invariants, `gl.error("step")` for messages |
| Letting a queued error grow stale | The queue is cumulative, so a later check reports an earlier failure | Drain with `clear_all()`, or check right after the group of calls |
| Calling `uniform` every frame | Each by-name setter looks the location up in the driver | Cache `location = program.uniform("mvp")` and use `set_vec_at`/`uniform_mat4` |
| Setting textures per unit without `active_texture` | You rebind unit 0 every time | `gl.active_texture(gl.texture_unit(n))` or `gl.bind_unit(n, id)` |
| Deleting a shader before linking | `link()` fails and the log explains why | Attach, link, then delete the stages if you no longer need them |
| Multi-window apps | One context per process; a context is current only on the thread that bound it | Keep one window, or call `make_current()` before touching GL from another thread |
| `destroy()` then more calls | `handle` is `None`; every call is a safe no-op | Destroy last, after the loop |
| Treating `dt()` as bounded | A dragging window or a breakpoint can produce a huge delta, and the first frame is a large value | Clamp it (`if delta > 0.1: delta = 0.1`) |
| Uploading `matrix.Mat4.data` directly | `matrix` is row-major and a `mat4` uniform is column-major, so the shader receives the transpose: a translation becomes a shear-like displacement and a rotation turns the wrong way | `gl.set_matrix4(program, name, m)` or `gl.matrix4_data(m)` |
| Multiplying transforms in the wrong order | `matrix.mul(a, b)` is `a * b`, so **b applies to the point first**; `mul(scale, translate)` scales the translation | Read `compose([...])` left to right, with the rightmost entry applied first |
| Passing degrees where radians are wanted | `matrix.rotate_y(30.0)` is 30 radians, which is not a small rotation | `gl.to_radians(30.0)`, or `gl.perspective_deg` when a field of view is in degrees |
| A uniform set by a misspelled name | The location is `-1` and the driver ignores it, so nothing happens and no error is raised | Check `program.uniform("name") != -1` while bringing a shader up |
| Lighting a scaled model without the normal matrix | Non-uniform scale shears the normals, so the shading drifts with the model's scale | `gl.set_normal_matrix(program, name, model)` |
| Comparing a projection against hand-written numbers | `perspective` follows the spec but is stored row-major here, so `data[11]` is not the value a column-major listing shows | Compare through `gl.matrix4_data(...)`, or test behaviour (see `tests/features/gl_math.sere`) |

## Testing

`tests/features/gl_binding.sere` compiles and runs the binding without creating a
window: it asserts enumerant values, the pure helpers (`error_name`,
`framebuffer_status_name`, `texture_unit`, `pixel_at` via a generated image),
`rgba_pixels`, and the context-free queries. It runs on the LLVM backend:

```
ctest --preset windows-clang-cl-relwithdebinfo -R gl_binding --output-on-failure
```

`tests/features/gl_math.sere` does the same for the math layer, and it needs no
context either, which is the point: projections, camera placement, orbiting,
transform composition, the normal matrix, screen-space projection, rays, and the
upload layout are all arithmetic. It pins the transpose convention down, so a
change to `matrix4_data` cannot silently flip every rotation:

```
ctest --preset windows-clang-cl-relwithdebinfo -R gl_math --output-on-failure
```

The Serem backend is skipped for both tests through `SERE_LLVM_ONLY_FEATURES` in
`tests/CMakeLists.txt`, because it cannot yet lower package member access
(`gl.CONSTANT` on an imported package); the entry lives in one place so it is
easy to remove when that lands.

The examples on this page are compiled, not just written:

```
py scripts\check-doc-examples.py docs/gl.md
```

That script compiles every ```` ```sere ```` block as a standalone program, so an
API rename cannot leave the documentation behind. A block that is deliberately a
fragment rather than a program is marked ```` ```sere ignore ````.

Tests that need a real context — shader compilation, uploading a mesh,
framebuffer completeness — belong in a program you run by hand, not in the
feature suite, because they need a desktop session. `_scratch/gl_probe.sere` in
an earlier session is the shape to imitate:

```sere
import gl

def main() -> i32:
    window: gl.Window = gl.Window("probe", 320, 200, gl.WINDOW_RESIZABLE)
    if not window.is_open():
        print("no window on this host")
        return 1
    window.make_current()
    print(gl.version(), gl.renderer(), len(gl.extensions()))
    gl.require("context")
    window.destroy()
    return 0
```

## Where the code lives

| Path | Role |
| --- | --- |
| `stdlib/gl/*.sere` | The binding: one module per area, plus the barrel |
| `stdlib/gl/transform.sere` | The math layer: projections, `Camera`, `Transform`, uploads, picking |
| `stdlib/gl/README.md` | Short overview, conventions, and a first program |
| `stdlib/matrix.sere` | The matrix math underneath: `Mat4`, `rotate_axis`, `inverse3`, `upper3` |
| `stdlib/vec.sere` | `Vec2`/`Vec3`/`Vec4`, which the math layer takes and returns |
| `scripts/gen-gl-constants.py` | Writes `stdlib/gl/constants.sere` from tables of spec values |
| `scripts/gen-gl-init.py` | Writes `stdlib/gl/__init__.sere` from the public names in the parts |
| `scripts/check-doc-examples.py` | Compiles every `sere` block in the documentation |
| `runtime/sere_gl.c` | Windows/WGL layer, the entry-point loader, and the stubs for other hosts |
| `runtime/sere_rt.h` | The declarations the binding's `extern "C"` names match |
| `tests/features/gl_binding.sere` | The context-free binding regression test |
| `tests/features/gl_math.sere` | The context-free math regression test |

Every one of the 184 `sere_gl_*` functions is bound. If you add a runtime
function, bind it in the matching part, then regenerate:

```
py scripts\gen-gl-constants.py
py scripts\gen-gl-init.py
```

A quick check that nothing is missing compares the two lists:

```
py -c "import re,glob;d=set(re.findall(r'sere_gl_[a-z0-9_]+',open('runtime/sere_rt.h',encoding='utf-8').read()));b=set();[b.update(re.findall(r'extern \"C\" \"(sere_gl_[a-z0-9_]+)\"',open(f,encoding='utf-8').read())) for f in glob.glob('stdlib/gl/*.sere')];print(len(d),len(b),sorted(d-b))"
```

A capability report is a useful first program on an unfamiliar machine, and it
exercises the query layer end to end:

```sere
import gl

def main() -> i32:
    window: gl.Window = gl.Window("info", 320, 200)
    if not window.is_open():
        print("no context")
        return 1
    window.make_current()

    print("gl      ", gl.version())
    print("vendor  ", gl.vendor())
    print("renderer", gl.renderer())
    print("glsl    ", gl.shading_language_version())
    print("extensions", len(gl.extensions()), gl.supports("GL_ARB_texture_float"))
    print("max texture", gl.max_texture_size(), "units", gl.get_integer(gl.GL_MAX_TEXTURE_UNITS))
    print("viewport", gl.max_viewport_dims())
    print("line width", gl.line_width_range())
    print("display", gl.screen_size(), "scale", window.content_scale())

    window.destroy()
    return 0
```

For shaping a program around the standard library — numeric helpers in
`arrays`/`matrix` for transforms, `random` for sampling, `fs` for assets — see
[stdlib.md](stdlib.md) and [arrays.md](arrays.md). To ship native GL code with a
Sere library, see [libraries.md](libraries.md) and [slib.md](slib.md).
