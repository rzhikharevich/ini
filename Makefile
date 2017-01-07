RELEASE = no

CFLAGS = -Wall -Wextra
ifeq ($(RELEASE), yes)
	CFLAGS += -Ofast
else
	CFLAGS += -O0 -g
endif

all: ini.o
