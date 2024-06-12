#!/bin/sh

# Path to aesdsocket program
AESDSOCKET_BIN="/usr/bin/aesdsocket"

# Command line arguments for aesdsocket
AESDSOCKET_ARGS="-d"

AESDSOCKET_NAME="aesdsocket"

# Start aesdsocket
do_start() {
    start-stop-daemon --start --background -n $AESDSOCKET_NAME --exec $AESDSOCKET_BIN -- $AESDSOCKET_ARGS
}

# Stop aesdsocket
do_stop() {
    start-stop-daemon -K -n $AESDSOCKET_NAME
    
}

case "$1" in
    start)
        do_start
        ;;
    stop)
        do_stop
        ;;
    restart)
        do_stop
        sleep 1
        do_start
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac

exit 0