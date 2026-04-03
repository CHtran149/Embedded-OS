CC ?= gcc
#
LIBS = -lmosquitto -lpthread -lm

OBJS = main.o OLED_LCD_SSD1306.o fonts.o
TARGET = btl_app

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)