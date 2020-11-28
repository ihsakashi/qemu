/*
 * QEMU System Emulator
 *
 * Copyright (c) 2003-2020 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "sysemu/sysemu.h"

#ifdef CONFIG_DARWIN
#include <pthread.h>
#include <sys/event.h>
#include <stdlib.h>
#include <unistd.h>

const char __attribute__((section("__TEXT,__info_plist"))) info_plist[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
    "<plist version=\"1.0\">\n"
    "<dict>\n"
    "	<key>CFBundleIdentifier</key>\n"
    "	<string>org.qemu.QEMU</string>\n"
    "</dict>\n"
    "</plist>\n";

// http://mac-os-x.10953.n7.nabble.com/Ensure-NSTask-terminates-when-parent-application-does-td31477.html
static void *WatchForParentTermination(void *args)
{
    pid_t ppid = getppid(); // get parent pid

    int kq = kqueue();
    if (kq != -1) {
        struct kevent procEvent; // wait for parent to exit
        EV_SET(&procEvent, // kevent
               ppid, // ident
               EVFILT_PROC, // filter
               EV_ADD, // flags
               NOTE_EXIT, // fflags
               0, // data
               0); // udata
        kevent(kq, &procEvent, 1, &procEvent, 1, NULL);
    }

    exit(0);
    return NULL;
}
#endif // CONFIG_DARWIN

#ifdef CONFIG_SDL
#if defined(__APPLE__) || defined(main)
#include <SDL.h>
static int qemu_main(int argc, char **argv, char **envp);
int main(int argc, char **argv)
{
    return qemu_main(argc, argv, NULL);
}
#undef main
#define main qemu_main
#endif
#endif /* CONFIG_SDL */

#ifdef CONFIG_COCOA
#undef main
#define main qemu_main
#endif /* CONFIG_COCOA */

int main(int argc, char **argv, char **envp)
{
    qemu_init(argc, argv, envp);
#ifdef CONFIG_DARWIN
    pthread_t thread;
    pthread_create(&thread, NULL, WatchForParentTermination, NULL);
    pthread_detach(thread);
#endif
    qemu_main_loop();
    qemu_cleanup();

    return 0;
}
