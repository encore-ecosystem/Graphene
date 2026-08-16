#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

static void pause_ms(long milliseconds) {
    struct timespec duration = {
        milliseconds / 1000, (milliseconds % 1000) * 1000000
    };
    while (nanosleep(&duration, &duration) < 0 && errno == EINTR) {}
}

static void emit_event(int device, unsigned short type,
    unsigned short code, int value) {
    struct input_event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.code = code;
    event.value = value;
    write(device, &event, sizeof(event));
}

static void point(int device, int x, int y) {
    emit_event(device, EV_ABS, ABS_X, x);
    emit_event(device, EV_ABS, ABS_Y, y);
    emit_event(device, EV_SYN, SYN_REPORT, 0);
}

int main(int argc, char **argv) {
    if (argc != 5) return 1;
    int device = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (device < 0) return 2;
    ioctl(device, UI_SET_EVBIT, EV_KEY);
    ioctl(device, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(device, UI_SET_EVBIT, EV_ABS);
    ioctl(device, UI_SET_ABSBIT, ABS_X);
    ioctl(device, UI_SET_ABSBIT, ABS_Y);
    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.id.bustype = BUS_USB;
    strcpy(setup.name, "Graphene drag validation input");
    struct uinput_abs_setup absolute;
    memset(&absolute, 0, sizeof(absolute));
    absolute.code = ABS_X;
    absolute.absinfo.maximum = 3839;
    ioctl(device, UI_ABS_SETUP, &absolute);
    absolute.code = ABS_Y;
    absolute.absinfo.maximum = 2159;
    ioctl(device, UI_ABS_SETUP, &absolute);
    ioctl(device, UI_DEV_SETUP, &setup);
    ioctl(device, UI_DEV_CREATE);
    pause_ms(500);
    int x0 = atoi(argv[1]), y0 = atoi(argv[2]);
    int x1 = atoi(argv[3]), y1 = atoi(argv[4]);
    point(device, x0, y0);
    pause_ms(250);
    emit_event(device, EV_KEY, BTN_LEFT, 1);
    emit_event(device, EV_SYN, SYN_REPORT, 0);
    for (int step = 1; step <= 30; ++step) {
        point(device, x0 + (x1 - x0) * step / 30,
            y0 + (y1 - y0) * step / 30);
        pause_ms(16);
    }
    emit_event(device, EV_KEY, BTN_LEFT, 0);
    emit_event(device, EV_SYN, SYN_REPORT, 0);
    pause_ms(400);
    ioctl(device, UI_DEV_DESTROY);
    close(device);
    return 0;
}
