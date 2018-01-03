#include <stdio.h>
#include <sys/poll.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <signal.h>
#include <zconf.h>

#include <iostream>

#include "FrameListener.hpp"
#include "Banner.hpp"
#include "JpegEncoder.hpp"
#include "StreamClient.h"

// MSG_NOSIGNAL does not exists on OS X
#if defined(__APPLE__) || defined(__MACH__)
# ifndef MSG_NOSIGNAL
#   define MSG_NOSIGNAL SO_NOSIGPIPE
# endif
#endif

char buf[16777216];

static FrameListener gWaiter;


void print_usage(char **argv) {
    char *name = NULL;
    name = strrchr(argv[0], '/');

    printf("Usage: %s [OPTIONS]\n", (name ? name + 1: argv[0]));
    printf("Stream video from a device.\n");
    printf("  -u, --udid UDID\t\ttarget specific device by its 40-digit device UDID\n");
    printf("  -r, --resolution RESOLUTION\tdesired resolution <w>x<h>\n");
    printf("  -h, --help\t\t\tprints usage information\n");
    printf("\n");
}


bool parse_args(int argc, char **argv, const char **udid, int *port, const char **resolution) {
    if ( argc < 5 ) {
        // Currently the easiest way to make all arguments required
        print_usage(argv);
        return false;
    }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-u") || !strcmp(argv[i], "--udid")) {
            i++;
            if (!argv[i]) {
                print_usage(argv);
                return false;
            }
            *udid = argv[i];
            continue;
        }
        else if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--port")) {
            i++;
            if (!argv[i]) {
                print_usage(argv);
                return false;
            }
            *port = atoi(argv[i]);
            continue;
        }
        else if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "--resolution")) {
            i++;
            if (!argv[i]) {
                print_usage(argv);
                return false;
            }
            *resolution = argv[i];
            continue;
        }
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            print_usage(argv);
            return false;

        }
        else {
            print_usage(argv);
            return false;
        }
    }
    return true;
}


static void signal_handler(int signum) {
    switch (signum) {
        case SIGINT:
            printf("Received SIGINT, stopping\n");
            gWaiter.stop();
            break;
        case SIGTERM:
            printf("Received SIGTERM, stopping\n");
            gWaiter.stop();
            break;
        default:
            abort();
    }
}


static void setup_signal_handler() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    // we want to just ignore the SIGPIPE and get a EPIPE when socket is closed
    signal(SIGPIPE, SIG_IGN);
}


static ssize_t pumps(int fd, unsigned char* data, size_t length) {
    do {
        // SIGPIPE is set to ignored so we will just get EPIPE instead
        ssize_t wrote = write(fd, data, length);

        if (wrote < 0) {
            return wrote;
        }

        data += wrote;
        length -= wrote;
    }
    while (length > 0);
    return 0;
}


void parseResolution(const char* resolution, uint32_t* width, uint32_t* height) {
    std::string _resolution(resolution);
    size_t sep = _resolution.find("x");
    *width = std::stoul(_resolution.substr(0, sep).c_str());
    *height = std::stoul(_resolution.substr(sep+1, _resolution.length()).c_str());
}

int main(int argc, char **argv) {
    setbuf(stdin, buf);

    const char *udid = NULL;
    const char *resolution = NULL;
    int port = 0;

    setup_signal_handler();
    if ( !parse_args(argc, argv, &udid, &port, &resolution) ) {
        return EXIT_FAILURE;
    }

    uint32_t width = 0, height = 0;
    parseResolution(resolution, &width, &height);

    StreamClient client;
    if (!client.setupDevice(udid)) {
        return EXIT_FAILURE;
    }
    client.setResolution(width, height);
    client.setFrameListener(&gWaiter);
    client.start();

    if (gWaiter.waitForFrameLimitTime(64) <= 0) {
        return EXIT_SUCCESS;
    }
    Frame frame;

    client.lockFrame(&frame);
    std::cerr << "resolution: " << frame.width << "x" << frame.height << std::endl;
    JpegEncoder encoder(&frame);

    DeviceInfo realInfo, desiredInfo;
    realInfo.orientation = 0;
    realInfo.height = height;
    realInfo.width = width;
    desiredInfo.orientation = 0;
    desiredInfo.height = height;
    desiredInfo.width = width;

    Banner banner(realInfo, desiredInfo);
    client.releaseFrame(&frame);

    unsigned char frameSize[4];
    int ppid = getppid();

    while (gWaiter.isRunning() > 0) {
        std::cerr << "New client connection" << std::endl;
        ppid = getppid();
        if (ppid == 1) {
            break;
        }
        //send(socket, banner.getData(), banner.getSize(), 0);
        write(1, banner.getData(), banner.getSize());

        while (gWaiter.isRunning() and gWaiter.waitForFrame() > 0) {
            ppid = getppid();
            if (ppid == 1) {
                break;
            }
            client.lockFrame(&frame);
            encoder.encode(&frame);
            client.releaseFrame(&frame);
            putUInt32LE(frameSize, encoder.getEncodedSize());
            pumps(1, frameSize, 4);
            pumps(1, encoder.getEncodedData(), encoder.getEncodedSize());

            struct pollfd fds;
            int ret;
            fds.fd = 0; /* this is STDIN */
            fds.events = POLLIN;
            ret = poll(&fds, 1, 0);
            if(ret == 1) {
                char newResolution[1000];
                uint32_t newW = 0, newH = 0;
                std::cin >> newResolution;
                parseResolution(newResolution, &newW, &newH);
                client.setResolution(newW, newH);
            }

        }

    }
    client.stop();
    return EXIT_SUCCESS;
}
