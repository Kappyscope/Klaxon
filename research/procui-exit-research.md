# Wii U ProcUI exit-to-menu: what real apps actually do

Researched 2026-11-15. Everything below is quoted from source I fetched, with the exact path.
Anything marked **inferred** is my reading, not a statement from the source.

## TL;DR — canonical sequence

```c
WHBProcInit();                 // = ProcUIInitEx(saveCb) ; MUST be before SDL_Init/WHBGfxInit
while (WHBProcIsRunning()) {   // pumps ProcUIProcessMessages(TRUE) every frame
    /* frame: input, draw, present */
}
// loop guard returned FALSE => whb already did:
//   RELEASE_FOREGROUND -> ProcUIDrawDoneRelease()
//   EXITING            -> ProcUIShutdown()
WHBProcShutdown();             // only relaunches for HBL title IDs; does NOT touch ProcUI
SDL_Quit();                    // WHBGfxShutdown()
return 0;                      // process ends -> OS launches the menu (default action)
```

`SYSLaunchMenu()` is **not** part of that sequence.

## 1. What wut/libwhb does — `libraries/libwhb/src/proc.c` (devkitPro/wut)

The whole state machine lives inside `WHBProcIsRunning()`:

```c
status = ProcUIProcessMessages(TRUE);
if (status == PROCUI_STATUS_EXITING) {
   WHBProcStopRunning();
} else if (status == PROCUI_STATUS_RELEASE_FOREGROUND) {
   ProcUIDrawDoneRelease();
}

if (!sRunning) {
   ProcUIShutdown();
}
return sRunning;
```

and the "shutdown" function is nearly empty:

```c
void WHBProcShutdown() {
   sRunning = FALSE;
   // If we're running from Homebrew Launcher we must do a SYSRelaunchTitle to
   // correctly return to HBL.
   if (sFromHBL) { SYSRelaunchTitle(0, NULL); }
}
```

Consequences (verified):
- `WHBProcShutdown()` never calls `ProcUIShutdown()`. The final `WHBProcIsRunning()` call is what shuts ProcUI down.
- `WHBProcShutdown()` calls `SYSRelaunchTitle(0, NULL)` **only** when the title ID is HBL / Mii Maker (`0x0005000013374842`, `...1004A000/A100/A200`). For a normal `.wuhb`/`.rpx` it is a no-op besides clearing the flag.
- `WHBProcInit()` calls `OSEnableHomeButtonMenu(FALSE)` + registers `PROCUI_CALLBACK_HOME_BUTTON_DENIED` for those same HBL title IDs.

## 2. Official app template — `devkitPro/wiiu-examples` `templates/application/source/main.c`

```c
WHBProcInit();
WHBLogConsoleInit();
while(WHBProcIsRunning()) { ... }
WHBLogConsoleFree();
WHBProcShutdown();
return 0;
```

No `SYSLaunchMenu()`. This is the shape devkitPro ships.

Note: `samples/procui` **no longer exists** in `devkitPro/wut` (the repo tree has no `samples/` at all; procui.h moved to `include/proc_ui/procui.h`). `devkitPro/wiiu-examples` has no procui sample either — only the two templates. So the sample named in the brief is gone; the template above is the replacement.

## 3. Is SYSLaunchMenu() required?

`include/sysapp/launch.h` (wut), verbatim docs:

```c
/**
 * Launches the HOME menu when the current application exits, and requests the
 * application exit immediately (through ProcUI).
 *
 * \note
 * This is the default behaviour upon application exit.
 */
void SYSLaunchMenu();
```

```c
/**
 * Restarts the current title ... once the running application quits, and
 * requests the application exit immediately (through ProcUI).
 */
void SYSRelaunchTitle(uint32_t argc, char *pa_Argv[]);
```

So:
- **Not required** to get back to the menu — that is the default on exit.
- It is **asynchronous**: it only *requests* exit through ProcUI. You still owe the ProcUI loop until `PROCUI_STATUS_EXITING`, then `ProcUIShutdown()`, then return.
- Call it **once**, and never from a second place "to be safe". maschell's post: *"If you want to open the Wii U Menu, simply call `SYSLaunchMenu()` **once**. This will trigger a ProcUI event, which the application will then react to."*

## 4. EXITING vs RELEASE_FOREGROUND

From `include/proc_ui/procui.h`:

- `PROCUI_STATUS_EXITING`: *"The application must release all resources, call ProcUIShutdown(), and exit."*
- `PROCUI_STATUS_RELEASE_FOREGROUND`: *"The current application must release all foreground-only resources, calling ProcUIDrawDoneRelease() when it's ready to go into the background."*
- `ProcUIShutdown()`: *"Shut down the ProcUI library for the current application. This should be called before the app exits."*

Real implementation, `wiiu-env/launchiine` `src/Application.cpp` — the example maschell's post points at:

```cpp
switch (ProcUIProcessMessages(true)) {
    case PROCUI_STATUS_EXITING: {
        exitCode = EXIT_SUCCESS; exitApplication = true; break;
    }
    case PROCUI_STATUS_RELEASE_FOREGROUND: {
        if (video != nullptr) {
            video->tvEnable(true); video->drcEnable(true);
            delete fontSystem; fontSystem = nullptr;
            delete video;      video = nullptr;
            libgui_memoryRelease();      // free MEM1 / foreground bucket
            ProcUIDrawDoneRelease();
        } else { ProcUIDrawDoneRelease(); }
        break;
    }
    case PROCUI_STATUS_IN_FOREGROUND: {
        if (!quitRequest) {
            if (video == nullptr) { libgui_memoryInitialize(); video = new CVideo(...); ... }
            executeProcess = true;
        }
        break;
    }
}
```

`ProcUIInit` in the ctor, `ProcUIShutdown()` in the dtor. Exit = leave the loop, return from `main`. No `SYSLaunchMenu()` anywhere in launchiine.

## 5. Does WHBProcShutdown() do the right thing?

It "does the right thing" only if you have already had a `WHBProcIsRunning()` return FALSE (that call did `ProcUIShutdown()`), or if you call `ProcUIShutdown()` yourself. It is **not** a ProcUI teardown function despite the name — it is an HBL-exit helper plus a flag clear.

Pattern that breaks: call `WHBProcShutdown()` (or `exit()`, or `return`) while ProcUI is still live → the OS never receives the exit acknowledgement.

## 6. RetroArch — `frontend/drivers/platform_wiiu.c` + `os/wiiu/main.c`

- `main_setup()`: `ProcUIInit(&SaveCallback);`
- `main_teardown()`: `deinit_pad_libraries(); ProcUIShutdown(); ...` — runs after `main_exit(NULL)` returns.
- `main()` ends with a comment about returning 0, then `_start()` (RPX entry) does:

```c
main(argc, argv);
fsdev_exit();
__fini();
memoryRelease();
SYSRelaunchTitle(0, 0);
exit(0);
```

No `SYSLaunchMenu()` in the driver, and no `ProcUIProcessMessages` in `platform_wiiu.c`, `runloop.c`, or `gfx/drivers/gx2_gfx.c` (all grepped). **Inferred / unresolved:** I did not find where RetroArch polls ProcUI in the files I read; I did not clone the full tree, so I can't claim it doesn't. What is verified: it shuts ProcUI down in teardown and relaunches a title at process end.

## 7. SDL2 Wii U port — the important correction

`yawut/SDL` branch `wiiu-2.0.9`, `src/video/wiiu/SDL_wiiuvideo.c` (identical code at `rw-r-r-0644/SDL`, and at `yawut/SDL` master):

```c
static int using_whb_proc = 0;

static int WIIU_VideoInit(_THIS) {
    if (!ProcUIIsRunning()) {
        WHBProcInit();
        using_whb_proc = 1;
    }
    WHBGfxInit();
    ...
}

static void WIIU_VideoQuit(_THIS) {
    wiiuFreeTextureShader();
    WHBGfxShutdown();
    if (using_whb_proc) WHBProcShutdown();
}

static void WIIU_PumpEvents(_THIS) { }   // empty
```

Facts:
- SDL inits ProcUI (`via WHBProcInit`) **only if the app has not already done so**, and only then shuts it down on `SDL_VideoQuit`.
- `WIIU_PumpEvents` is empty and there is **no** `SYSLaunchMenu` anywhere in the Wii U files of either port.
- `src/video/wiiu/SDL_wiiuvideo.c` self-describes as *"basically just a stub at this point - all the magic happens in SDL_Render"*.

**Therefore: SDL2-on-Wii-U does not deliver `SDL_QUIT` on `PROCUI_STATUS_EXITING`, does not call `SYSLaunchMenu()`, and does not pump ProcUI.** Any design that says "SDL owns ProcUI, so I just return" is unfounded for these ports. An SDL app must run its own `WHBProcIsRunning()` (or raw ProcUI) guard every frame, *before* `SDL_Init`.

## 8. CoffeeShop

- `maschell/CoffeeShop` **is 404 / gone** (checked repo API: `"Not Found"`). The live SDL2 `.wuhb` mod manager is `timkicker/coffeeshop` (C++17, SDL2, `wut_create_wuhb` per its `CMakeLists.txt`), and it is where the SDL comments come from.
- Its `src/main.cpp` / `src/app/App.cpp` contain the most explicit description of the hang I found anywhere, but it **contradicts the SDL source**:

```cpp
// NOTE: We do NOT call WHBProcInit() here. SDL_Init() will detect that
// ProcUI isn't running yet and call ProcUIInitEx() itself, claiming
// ownership of the ProcUI lifecycle. With handleProcUI=TRUE on its side,
// SDL drives ProcUI from PumpEvents (sending SDL_QUIT on EXITING) and
// properly drains it on SDL_Quit (SYSLaunchMenu + ProcUIProcessMessages
// loop) ... -- which is exactly the rabbit hole that produced the
// HOME-exit hang. See SDL_wiiuvideo.c:WIIU_VideoInit / WIIU_VideoQuit.
```

and, the load-bearing one for the symptom:

```cpp
void App::startExit() {
    // ... SDL2-wuhb is the single owner of the ProcUI/SYSLaunchMenu handshake ...
    // Calling SYSLaunchMenu here too would issue it twice in flight
    // (once now, once during SDL_Quit) and reproduce the original
    // "Wii U Menu loading" hang.
}
```

**Inferred:** this project's comments describe a patched/local SDL ("SDL2-wuhb", `handleProcUI`) that is *not* present in either public Wii U SDL port. Treat the "SDL does the exit dance" premise as unverified, and the "second `SYSLaunchMenu()` in flight hangs the menu transition" observation as a project report consistent with an async launch request being issued twice.

## 9. Causes of "stuck on the Wii U Menu transition screen"

Verified statement of the mechanism, maschell, *Why some homebrew apps are incompatible with Aroma* (2023-04-30):

> "If the application doesn't react to the request to transit to the background or exit, the OS is still waiting for a response, even if the application has actually already exited, resulting in a softlock."

> "On the Wii U, you never just return from the `main` function when you want to exit the application. The OS always needs to know what to launch next, and exiting always happens via the ProcUI-loop."

Concrete causes, ranked:

1. **ProcUI never shut down** — app returns from `main`/`exit()` while ProcUI is still live (no `ProcUIShutdown()`, no final `WHBProcIsRunning()`). OS waits for the answer that never comes; the menu transition spins. This is the exact case maschell describes.
2. **Blocking after the handshake** — `ProcUIShutdown()` (or `SYSRelaunchTitle`) happens, then teardown blocks (thread join, `SDL_Quit`/`WHBGfxShutdown`, network close, `ACClose`, `socket_lib_finish`). The process never ends, so the launch never proceeds. **Inferred** from the fact that exit is "requested" and only completes when the app quits.
3. **Duplicate launch request** — two `SYSLaunchMenu()` calls in flight (e.g. a UI "quit" plus an SDL/other teardown path). Reported by timkicker/coffeeshop; mechanism plausible because the request is async and idempotency is undocumented.
4. **No ProcUI loop at all** — the classic HBL-era `.rpx`: works under HBL (home button = exit), softlocks as an installed title/Aroma.

## 10. Dead ends (so nobody repeats them)

- `rlaphoenix/NTON` — **not a Wii U app.** `rom/source/main.c` is Nintendo Switch homebrew (`svcSendSyncRequest`, `romfsExit`, `pmshellInitialize`, `__wrap_exit`). No ProcUI content. The brief's guess was wrong.
- `maschell/CoffeeShop` — 404, repo no longer public/renamed.
- wut `samples/procui` — removed upstream.

## Sources (read directly)

Code:
- https://raw.githubusercontent.com/devkitPro/wut/master/libraries/libwhb/src/proc.c
- https://raw.githubusercontent.com/devkitPro/wut/master/libraries/libwhb/include/whb/proc.h
- https://raw.githubusercontent.com/devkitPro/wut/master/include/proc_ui/procui.h
- https://raw.githubusercontent.com/devkitPro/wut/master/include/sysapp/launch.h
- https://raw.githubusercontent.com/devkitPro/wiiu-examples/master/templates/application/source/main.c
- https://raw.githubusercontent.com/wiiu-env/launchiine/master/src/Application.cpp
- https://raw.githubusercontent.com/wiiu-env/launchiine/master/src/entry.cpp
- https://raw.githubusercontent.com/libretro/RetroArch/master/frontend/drivers/platform_wiiu.c
- https://raw.githubusercontent.com/libretro/RetroArch/master/os/wiiu/main.c
- https://raw.githubusercontent.com/yawut/SDL/wiiu-2.0.9/src/video/wiiu/SDL_wiiuvideo.c
- https://raw.githubusercontent.com/rw-r-r-0644/SDL/wiiu-2.0.9/src/video/wiiu/SDL_wiiuvideo.c
- https://raw.githubusercontent.com/timkicker/coffeeshop/main/src/main.cpp
- https://raw.githubusercontent.com/timkicker/coffeeshop/main/src/app/App.cpp
- https://raw.githubusercontent.com/timkicker/coffeeshop/main/CMakeLists.txt

Prose:
- https://maschell.github.io/homebrew/2023/04/30/incompatible-homebrews.html

Local copies of everything: `research/_src/`
