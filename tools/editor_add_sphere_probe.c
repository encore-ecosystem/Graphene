#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdbool.h>
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

static void point_and_click(int device, int x, int y) {
    emit(device, EV_ABS, ABS_X, x);
    emit(device, EV_ABS, ABS_Y, y);
    emit(device, EV_SYN, SYN_REPORT, 0);
    pause_ms(180);
    emit(device, EV_KEY, BTN_LEFT, 1);
    emit(device, EV_SYN, SYN_REPORT, 0);
    pause_ms(40);
    emit(device, EV_KEY, BTN_LEFT, 0);
    emit(device, EV_SYN, SYN_REPORT, 0);
    pause_ms(300);
}

int main(int argc, char **argv) {
    int device = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (device < 0) return 1;
    if (ioctl(device, UI_SET_EVBIT, EV_KEY) < 0 ||
        ioctl(device, UI_SET_KEYBIT, BTN_LEFT) < 0 ||
        ioctl(device, UI_SET_EVBIT, EV_ABS) < 0 ||
        ioctl(device, UI_SET_ABSBIT, ABS_X) < 0 ||
        ioctl(device, UI_SET_ABSBIT, ABS_Y) < 0) {
        close(device);
        return 2;
    }
    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.id.bustype = BUS_USB;
    setup.id.vendor = 0x2e;
    setup.id.product = 0x4751;
    strcpy(setup.name, "Graphene add sphere validation input");
    struct uinput_abs_setup absolute;
    memset(&absolute, 0, sizeof(absolute));
    absolute.code = ABS_X;
    absolute.absinfo.maximum = 3839;
    if (ioctl(device, UI_ABS_SETUP, &absolute) < 0) {
        close(device);
        return 3;
    }
    absolute.code = ABS_Y;
    absolute.absinfo.maximum = 2159;
    if (ioctl(device, UI_ABS_SETUP, &absolute) < 0 ||
        ioctl(device, UI_DEV_SETUP, &setup) < 0 ||
        ioctl(device, UI_DEV_CREATE) < 0) {
        close(device);
        return 3;
    }
    pause_ms(500);
    // Graphene is maximized on the 2256x2160 left output. Open the Outliner
    // add menu and choose the third basic primitive.
    if (argc == 3) {
        point_and_click(device, atoi(argv[1]), atoi(argv[2]));
    } else if (argc > 1 && strcmp(argv[1], "sphere") == 0) {
        point_and_click(device, 120, 410);
    } else {
        point_and_click(device, 300, 283);
        if (argc == 1 || strcmp(argv[1], "menu") != 0) {
            point_and_click(device, 120, 410);
        }
    }
    ioctl(device, UI_DEV_DESTROY);
    close(device);
    return 0;
}
