# BlockOS / LXQt session status (step 44)

## Status
lxqt-session does NOT run on BlockOS yet. Not ready for the 2026-11-24 deadline.

## Works
- libdbus 1.15.8 core and QtDBus 5.15 compile and link (static, BlockOS).
- TLS bug fixed in the probe (tls_clone_fix.c): new threads no longer inherit parent TLS values.
- Qt 5 widgets render offscreen (label visible).
- QThread start, cross-thread postEvent, plain pthread_create + malloc in threads (screenshot 04).

## Open bug (not solved)
QtDBus / BlockingQueuedConnection crashes. Trace so far:
- qt_initialize_pthread_cond gets cond=4, i.e. QWaitConditionPrivate* == NULL
  (QWaitCondition ctor, qwaitcondition_unix.cpp:175-177).
- Debugging shows this->d is overwritten to 0 right after pthread_mutex_init(d).
  Working hypothesis: malloc returned a block overlapping the live QWaitCondition object
  (heap aliasing). NOT proven. Needs a multi-threaded malloc stress test with duplicate-address check.
- heap_lock (libc stdlib.c) is a spinlock; reviewed, no obvious bug found.
- Qt build uses OLD libc headers (scratchpad/bos/userspace/libc/include): pthread_mutex_t = 4 bytes.
  The current bos31 headers have 16 bytes. This ABI mismatch must be resolved.

## Known bugs
- taskbar.cpp: /devices/display stride is in PIXELS, code treats it as bytes. Dropped.
- Keyboard: first key after boot is lost.
- Thread stdio: duplicated stdout line seen when two threads printf (no stdio locking).

## Stubs / temporary
- Box-glyph font DB instead of FreeType (bos_freetype_minimal.cpp).
- libdbus gen15 shims: getnameinfo -1, getgrnam_r/getgrgid_r empty, setgroups ENOSYS.
- dbus-daemon not run; session bus address /tmp/dbus-session.
- SessionApplication not yet compiled against QtDBus (X11 parts wrapped out).
- Debug shims in src/: qtinit_dbg.cpp (replaces qt_initialize_pthread_cond, logs bad cond),
  wrapnew.cpp (wraps operator new, logs NULL allocations, fallback arena). Debug only.

## Roadmap (honest)
1. Prove or disprove heap aliasing: multi-threaded malloc stress with duplicate-address check.
2. Fix libc headers ABI (pthread_mutex_t size) and rebuild all Qt objects against the one libc.
3. Make BlockingQueued and QThread-with-event-loop work (blockq / loopq probes).
4. Run dbus-daemon on BlockOS; then sessionBus() connect.
5. Compile the real SessionApplication against QtDBus; then lxqt-session.
6. Real FreeType + fonts; native framebuffer output (fix stride-as-pixels).
