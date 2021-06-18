gcc -pthread `pkg-config gtk+-3.0 --cflags` battery_test.c -o battery `pkg-config gtk+-3.0 --libs` -rdynamic
