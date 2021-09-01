#include <sys/ioctl.h>
#include <unistd.h>

size_t get_terminal_width() {
  winsize window_size;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size);
  return window_size.ws_col;
}