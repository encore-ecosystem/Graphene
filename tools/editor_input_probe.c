#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

static void pause_ms(long milliseconds) {
    struct timespec duration = {
        milliseconds / 1000,
        (milliseconds % 1000) * 1000000
    };
    while (nanosleep(&duration, &duration) < 0 && errno == EINTR) {
    }
}

static bool emit(int device, unsigned short type,
    unsigned short code, int value) {
    struct input_event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.code = code;
    event.value = value;
    return write(device, &event, sizeof(event)) == sizeof(event);
}

static bool sync_events(int device) {
    return emit(device, EV_SYN, SYN_REPORT, 0);
}

static void click_absolute(int device, int x, int y) {
    emit(device, EV_ABS, ABS_X, x);
    emit(device, EV_ABS, ABS_Y, y);
    sync_events(device);
    // Some Wayland compositors update the visible cursor for an absolute
    // event before delivering surface-local motion. A tiny relative nudge
    // guarantees Luma receives a fresh hover target from this hotplugged
    // validation device.
    emit(device, EV_REL, REL_X, 1);
    sync_events(device);
    emit(device, EV_REL, REL_X, -1);
    sync_events(device);
    // Let Luma publish the hover/hit-test target at the new pointer position
    // before the button edge is delivered.
    pause_ms(500);
    emit(device, EV_KEY, BTN_LEFT, 1);
    sync_events(device);
    pause_ms(60);
    emit(device, EV_KEY, BTN_LEFT, 0);
    sync_events(device);
}

int main(int argc, char **argv) {
    int cycles = argc > 1 ? atoi(argv[1]) : 1;
    bool hold_device = argc > 3 && strcmp(argv[3], "hold") == 0;
    bool open_recent_project = argc > 2 &&
        strcmp(argv[2], "open-recent") == 0;
    bool move_only = argc > 2 && strcmp(argv[2], "move-only") == 0;
    bool down_only = argc > 2 && strcmp(argv[2], "down-only") == 0;
    bool open_view_menu = argc > 2 &&
        strcmp(argv[2], "open-view-menu") == 0;
    bool select_clusters = argc > 2 &&
        strcmp(argv[2], "select-clusters") == 0;
    const char *view_mode = argc > 2 &&
        strncmp(argv[2], "mode-", 5) == 0 ? argv[2] + 5 : NULL;
    const char *open_view_mode = argc > 2 &&
        strncmp(argv[2], "choose-", 7) == 0 ? argv[2] + 7 : NULL;
    bool deselect_viewport = argc > 2 &&
        strcmp(argv[2], "deselect") == 0;
    bool select_sphere = argc > 2 &&
        strcmp(argv[2], "select-sphere") == 0;
    bool overlap_sphere = argc > 2 &&
        strcmp(argv[2], "overlap-sphere") == 0;
    bool play_toggle = argc > 2 &&
        strcmp(argv[2], "play-toggle") == 0;
    bool hover_clusters = argc > 2 &&
        strcmp(argv[2], "hover-clusters") == 0;
    bool focus_sphere = argc > 2 &&
        strcmp(argv[2], "focus-sphere") == 0;
    bool zoom_far = argc > 2 &&
        strcmp(argv[2], "zoom-far") == 0;
    bool zoom_near = argc > 2 &&
        strcmp(argv[2], "zoom-near") == 0;
    bool flight_back = argc > 2 &&
        strcmp(argv[2], "flight-back") == 0;
    bool clear_outliner_search = argc > 2 &&
        strcmp(argv[2], "clear-search") == 0;
    bool duplicate_selected = argc > 2 &&
        strcmp(argv[2], "duplicate-selected") == 0;
    bool nudge_x = argc > 2 &&
        strcmp(argv[2], "nudge-x") == 0;
    bool align_x = argc > 2 &&
        strcmp(argv[2], "align-x") == 0;
    bool nudge_z = argc > 2 &&
        strcmp(argv[2], "nudge-z") == 0;
    bool shrink_selected = argc > 2 &&
        strcmp(argv[2], "shrink-selected") == 0;
    bool move_z_other_side = argc > 2 &&
        strcmp(argv[2], "move-z-other-side") == 0;
    bool nudge_z_plus = argc > 2 &&
        strcmp(argv[2], "nudge-z-plus") == 0;
    bool click_point = argc > 4 && strcmp(argv[2], "click-point") == 0;
    bool move_point = argc > 4 && strcmp(argv[2], "move-point") == 0;
    if (cycles < 1) cycles = 1;
    int device = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (device < 0) return 1;
    if (ioctl(device, UI_SET_EVBIT, EV_KEY) < 0 ||
        ioctl(device, UI_SET_KEYBIT, BTN_LEFT) < 0 ||
        ioctl(device, UI_SET_KEYBIT, BTN_RIGHT) < 0 ||
        ioctl(device, UI_SET_KEYBIT, KEY_W) < 0 ||
        ioctl(device, UI_SET_KEYBIT, KEY_S) < 0 ||
        ioctl(device, UI_SET_KEYBIT, KEY_D) < 0 ||
        ioctl(device, UI_SET_KEYBIT, KEY_F) < 0 ||
        ioctl(device, UI_SET_KEYBIT, KEY_Q) < 0 ||
        ioctl(device, UI_SET_KEYBIT, KEY_E) < 0 ||
        ioctl(device, UI_SET_KEYBIT, KEY_LEFTCTRL) < 0 ||
        ioctl(device, UI_SET_EVBIT, EV_ABS) < 0 ||
        ioctl(device, UI_SET_ABSBIT, ABS_X) < 0 ||
        ioctl(device, UI_SET_ABSBIT, ABS_Y) < 0 ||
        ioctl(device, UI_SET_EVBIT, EV_REL) < 0 ||
        ioctl(device, UI_SET_RELBIT, REL_X) < 0 ||
        ioctl(device, UI_SET_RELBIT, REL_Y) < 0 ||
        ioctl(device, UI_SET_RELBIT, REL_WHEEL) < 0) {
        close(device);
        return 2;
    }
    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.id.bustype = BUS_USB;
    setup.id.vendor = 0x2e;
    setup.id.product = 0x4750;
    strcpy(setup.name, "Graphene editor validation input");
    struct uinput_abs_setup absolute;
    memset(&absolute, 0, sizeof(absolute));
    absolute.code = ABS_X;
    // Keep one common range: the compositor maps it into the scaled logical
    // coordinate space of the left output.
    absolute.absinfo.maximum = 3839;
    if (ioctl(device, UI_ABS_SETUP, &absolute) < 0) {
        close(device);
        return 3;
    }
    absolute.code = ABS_Y;
    absolute.absinfo.maximum = 3839;
    absolute.absinfo.maximum = 2159;
    if (ioctl(device, UI_ABS_SETUP, &absolute) < 0 ||
        ioctl(device, UI_DEV_SETUP, &setup) < 0 ||
        ioctl(device, UI_DEV_CREATE) < 0) {
        close(device);
        return 3;
    }
    pause_ms(500);

    if (click_point) {
        click_absolute(device, atoi(argv[3]), atoi(argv[4]));
        pause_ms(900);
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 0;
    }

    if (move_point) {
        emit(device, EV_ABS, ABS_X, atoi(argv[3]));
        emit(device, EV_ABS, ABS_Y, atoi(argv[4]));
        sync_events(device);
        emit(device, EV_REL, REL_X, -1);
        sync_events(device);
        pause_ms(900);
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 0;
    }

    if (move_only) {
        emit(device, EV_ABS, ABS_X, 650);
        emit(device, EV_ABS, ABS_Y, 232);
        sync_events(device);
        while (hold_device) pause_ms(1000);
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 0;
    }
    if (down_only) {
        emit(device, EV_ABS, ABS_X, 650);
        emit(device, EV_ABS, ABS_Y, 232);
        sync_events(device);
        pause_ms(500);
        emit(device, EV_KEY, BTN_LEFT, 1);
        sync_events(device);
        while (hold_device) pause_ms(1000);
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 0;
    }

    if (open_recent_project) {
        // Open the first recent-project card on Graphene's welcome screen.
        click_absolute(device, 180, 680);
        pause_ms(4000);
    }
    if (open_view_menu || select_clusters || view_mode != NULL ||
        open_view_mode != NULL) {
        if (open_view_mode == NULL) {
            click_absolute(device, 650, 232);
            // The retained tree is rebuilt after opening the strip. Wait for
            // the new hit-test snapshot before clicking into that new tree.
            pause_ms(900);
        }
        if (select_clusters || view_mode != NULL || open_view_mode != NULL) {
            int mode_y = 464;
            const char *requested_mode =
                open_view_mode != NULL ? open_view_mode : view_mode;
            if (requested_mode != NULL) {
                const char *names[] = {
                    "lit", "unlit", "mask", "triangles", "patches",
                    "clusters", "primitives", "instances", "lod",
                    "residency", "overdraw", "occlusion"
                };
                // The current selector is a vertical absolute overlay. These
                // are physical centers on the scaled validation output.
                const int positions[] = {
                    287, 322, 357, 392, 428, 464, 500, 535, 570,
                    606, 642, 678
                };
                for (int mode = 0; mode < 12; ++mode) {
                    if (strcmp(requested_mode, names[mode]) == 0) {
                        mode_y = positions[mode];
                        break;
                    }
                }
            }
            click_absolute(device, 700, mode_y);
            pause_ms(700);
        } else if (open_view_menu) {
            pause_ms(5000);
        }
        while (hold_device) pause_ms(1000);
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 0;
    }
    if (hover_clusters) {
        click_absolute(device, 650, 232);
        pause_ms(900);
        emit(device, EV_ABS, ABS_X, 700);
        emit(device, EV_ABS, ABS_Y, 464);
        sync_events(device);
        pause_ms(700);
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 0;
    }
    if (focus_sphere) {
        // Sphere 3 is the fifth outliner row in the validation scene. Focus it
        // through the visible outliner and the standard viewport F shortcut.
        click_absolute(device, 135, 568);
        pause_ms(250);
        emit(device, EV_ABS, ABS_X, 1050);
        emit(device, EV_ABS, ABS_Y, 720);
        sync_events(device);
        pause_ms(120);
        emit(device, EV_KEY, KEY_F, 1);
        sync_events(device);
        pause_ms(40);
        emit(device, EV_KEY, KEY_F, 0);
        sync_events(device);
        pause_ms(700);
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 0;
    }
    if (clear_outliner_search) {
        click_absolute(device, 150, 283);
        emit(device, EV_KEY, KEY_LEFTCTRL, 1);
        sync_events(device);
        emit(device, EV_KEY, 30, 1);
        sync_events(device);
        emit(device, EV_KEY, 30, 0);
        sync_events(device);
        emit(device, EV_KEY, KEY_LEFTCTRL, 0);
        sync_events(device);
        emit(device, EV_KEY, KEY_BACKSPACE, 1);
        sync_events(device);
        emit(device, EV_KEY, KEY_BACKSPACE, 0);
        sync_events(device);
        pause_ms(700);
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 0;
    }
    if (zoom_far || zoom_near) {
        emit(device, EV_ABS, ABS_X, 1050);
        emit(device, EV_ABS, ABS_Y, 720);
        sync_events(device);
        pause_ms(120);
        // Graphene follows the viewport convention where positive wheel
        // motion dollies away from the orbit pivot. Keep the proof step small
        // enough that the selected mesh remains visible at its coarse LOD.
        int amount = zoom_far ? 6 : -6;
        emit(device, EV_REL, REL_WHEEL, amount);
        sync_events(device);
        pause_ms(900);
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 0;
    }
    if (flight_back) {
        emit(device, EV_ABS, ABS_X, 1050);
        emit(device, EV_ABS, ABS_Y, 720);
        sync_events(device);
        pause_ms(120);
        emit(device, EV_KEY, BTN_RIGHT, 1);
        emit(device, EV_KEY, KEY_W, 1);
        sync_events(device);
        for (int step = 0; step < 18; ++step) {
            emit(device, EV_REL, REL_X, step % 2 == 0 ? 1 : -1);
            emit(device, EV_REL, REL_Y, 0);
            sync_events(device);
            pause_ms(16);
        }
        emit(device, EV_KEY, KEY_W, 0);
        emit(device, EV_KEY, BTN_RIGHT, 0);
        sync_events(device);
        pause_ms(700);
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 0;
    }
    if (play_toggle) {
        click_absolute(device, 1308, 134);
        pause_ms(900);
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 0;
    }
    if (deselect_viewport) {
        click_absolute(device, 1450, 520);
        pause_ms(500);
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 0;
    }
    if (select_sphere) {
        click_absolute(device, 135, 522);
        pause_ms(500);
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 0;
    }
    if (overlap_sphere) {
        // Move the selected sphere left through the Details X decrement
        // control until its screen projection overlaps the cube.
        for (int step = 0; step < 8; ++step) {
            click_absolute(device, 1760, 540);
            pause_ms(45);
        }
        pause_ms(700);
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 0;
    }
    if (duplicate_selected) {
        // Toolbar duplicate button; using the visible control also validates
        // retained hit-testing instead of invoking editor state directly.
        click_absolute(device, 235, 134);
        pause_ms(900);
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 0;
    }
    if (nudge_x) {
        for (int step = 0; step < 4; ++step) {
            click_absolute(device, 1800, 540);
            pause_ms(45);
        }
        pause_ms(150);
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 0;
    }
    if (align_x) {
        for (int step = 0; step < 3; ++step) {
            click_absolute(device, 1760, 540);
            pause_ms(45);
        }
        pause_ms(150);
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 0;
    }
    if (nudge_z) {
        // Restore the one Y decrement used while locating the compact Details
        // row, then move the duplicate one Z step behind the source.
        click_absolute(device, 1940, 540);
        pause_ms(80);
        click_absolute(device, 2020, 540);
        pause_ms(150);
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 0;
    }
    if (shrink_selected) {
        const int axes[] = {1760, 1880, 2020};
        for (int repeat = 0; repeat < 2; ++repeat) {
            for (int axis = 0; axis < 3; ++axis) {
                click_absolute(device, axes[axis], 754);
                pause_ms(45);
            }
        }
        pause_ms(150);
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 0;
    }
    if (move_z_other_side) {
        for (int step = 0; step < 5; ++step) {
            click_absolute(device, 2080, 540);
            pause_ms(45);
        }
        pause_ms(150);
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 0;
    }
    if (nudge_z_plus) {
        // One visible Details click lets screenshot validation isolate the
        // exact frame in which a previously occluded instance is recovered.
        click_absolute(device, 2080, 540);
        pause_ms(150);
        ioctl(device, UI_DEV_DESTROY);
        close(device);
        return 0;
    }
    // Place the pointer inside the Graphene viewport on the left monitor.
    emit(device, EV_ABS, ABS_X, 1050);
    emit(device, EV_ABS, ABS_Y, 720);
    sync_events(device);
    pause_ms(150);

    int cycle = 0;
    while (cycle < cycles) {
        unsigned short movement = cycle % 4 == 0 ? KEY_W :
            (cycle % 4 == 1 ? KEY_D :
                (cycle % 4 == 2 ? KEY_Q : KEY_E));
        emit(device, EV_KEY, BTN_RIGHT, 1);
        emit(device, EV_KEY, movement, 1);
        sync_events(device);
        int step = 0;
        while (step < 18) {
            emit(device, EV_REL, REL_X, step % 2 == 0 ? 2 : -1);
            emit(device, EV_REL, REL_Y, step % 3 == 0 ? -1 : 1);
            sync_events(device);
            pause_ms(16);
            ++step;
        }
        emit(device, EV_KEY, movement, 0);
        emit(device, EV_KEY, BTN_RIGHT, 0);
        sync_events(device);
        if (cycle % 5 == 0) {
            emit(device, EV_REL, REL_WHEEL,
                cycle % 10 == 0 ? 1 : -1);
            sync_events(device);
        }
        if (cycle % 10 == 0) {
            // Keep the soak representative of an editor session: select an
            // actor and alternate move/rotate/scale edits in Details instead
            // of exercising only the camera.
            click_absolute(device, 135, 522);
            int transform_row = (cycle / 10) % 3;
            int transform_y = transform_row == 0 ? 540 :
                (transform_row == 1 ? 648 : 754);
            click_absolute(device,
                ((cycle / 10) % 2 == 0) ? 1800 : 1748, transform_y);
            emit(device, EV_ABS, ABS_X, 1050);
            emit(device, EV_ABS, ABS_Y, 720);
            sync_events(device);
            pause_ms(100);
        }
        pause_ms(700);
        ++cycle;
    }

    // Exercise unbounded viewport zoom, then refocus the selected actor so a
    // post-interaction screenshot still contains the rendered test scene.
    emit(device, EV_REL, REL_WHEEL, 2);
    sync_events(device);
    pause_ms(80);
    emit(device, EV_KEY, KEY_F, 1);
    sync_events(device);
    pause_ms(30);
    emit(device, EV_KEY, KEY_F, 0);
    sync_events(device);
    pause_ms(120);

    while (hold_device) pause_ms(1000);
    ioctl(device, UI_DEV_DESTROY);
    close(device);
    return 0;
}
