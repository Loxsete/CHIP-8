# Имя программы
TARGET = chip8

# Файлы с кодом
SOURCES = chip8.c

# Компилятор
CC = gcc

# Флаги компиляции: включаем SDL2 и math
CFLAGS = -Wall -O2 `sdl2-config --cflags`
LDFLAGS = `sdl2-config --libs` -lm

# Правило для сборки программы
$(TARGET): $(SOURCES)
	$(CC) $(SOURCES) $(CFLAGS) $(LDFLAGS) -o $(TARGET)

# Очистка
clean:
	rm -f $(TARGET)
