#include <napi.h>

#ifdef _WIN32
#include <windows.h>

int cursor_position(int *const rowptr, int *const colptr)
{
  HANDLE console_handle;
  CONSOLE_SCREEN_BUFFER_INFO console_info;

  console_handle = CreateFileW(L"CONOUT$",
                               GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL,
                               OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL,
                               NULL);

  if (console_handle == INVALID_HANDLE_VALUE)
    return GetLastError();

  if (!GetConsoleScreenBufferInfo(console_handle, &console_info))
    return GetLastError();

  /* Success! */

  if (rowptr)
    *rowptr = console_info.dwCursorPosition.Y + 1;

  if (colptr)
    *colptr = console_info.dwCursorPosition.X + 1;

  /* Done. */
  return 0;
}

#else

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

#define RD_EOF -1
#define RD_EIO -2

static inline int rd(const int fd)
{
  unsigned char buffer[4];
  ssize_t n;

  while (1)
  {
    n = read(fd, buffer, 1);

    if (n > (ssize_t)0)
      return buffer[0];
    else if (n == (ssize_t)0)
      return RD_EOF;
    else if (n != (ssize_t)-1)
      return RD_EIO;
    else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
      return RD_EIO;
  }
}

static inline int wr(const int fd, const char *const data, const size_t bytes)
{
  const char *head = data;
  const char *const tail = data + bytes;
  ssize_t n;

  while (head < tail)
  {

    n = write(fd, head, (size_t)(tail - head));

    if (n > (ssize_t)0)
      head += n;
    else if (n != (ssize_t)-1)
      return EIO;
    else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
      return errno;
  }

  return 0;
}

/* Return a new file descriptor to the current TTY. */
int current_tty(void)
{
  const char *dev;
  int fd;

  dev = ttyname(STDIN_FILENO);

  if (!dev)
    dev = ttyname(STDOUT_FILENO);

  if (!dev)
    dev = ttyname(STDERR_FILENO);

  if (!dev)
  {
    errno = ENOTTY;
    return -1;
  }

  do
  {
    fd = open(dev, O_RDWR | O_NOCTTY);
  } while (fd == -1 && errno == EINTR);

  if (fd == -1)
    return -1;

  return fd;
}

/* As the tty for current cursor position.
 * This function returns 0 if success, errno code otherwise.
 * Actual errno will be unchanged.
 */
int cursor_position(int *const rowptr, int *const colptr)
{
  struct termios saved, temporary;
  int tty, retval, result, rows, cols, saved_errno;

  tty = current_tty();

  /* Bad tty? */
  if (tty == -1)
    return ENOTTY;

  saved_errno = errno;

  /* Save current terminal settings. */
  do
  {
    result = tcgetattr(tty, &saved);
  } while (result == -1 && errno == EINTR);

  if (result == -1)
  {
    retval = errno;
    errno = saved_errno;
    return retval;
  }

  /* Get current terminal settings for basis, too. */
  do
  {
    result = tcgetattr(tty, &temporary);
  } while (result == -1 && errno == EINTR);

  if (result == -1)
  {
    retval = errno;
    errno = saved_errno;
    return retval;
  }

  /* Disable ICANON, ECHO, and CREAD. */
  temporary.c_lflag &= ~ICANON;
  temporary.c_lflag &= ~ECHO;
  temporary.c_cflag &= ~CREAD;

  /* This loop is only executed once. When broken out,
   * the terminal settings will be restored, and the function
   * will return retval to caller. It's better than goto.
   */
  do
  {
    /* Set modified settings. */
    do
    {
      result = tcsetattr(tty, TCSANOW, &temporary);
    } while (result == -1 && errno == EINTR);

    if (result == -1)
    {
      retval = errno;
      break;
    }

    /* Request cursor coordinates from the terminal. */
    retval = wr(tty, "\033[6n", 4);
    if (retval)
      break;

    /* Assume coordinate reponse parsing fails. */
    retval = EIO;

    /* Expect an ESC. */
    result = rd(tty);
    if (result != 27)
      break;

    /* Expect [ after the ESC. */
    result = rd(tty);
    if (result != '[')
      break;

    /* Parse rows. */
    rows = 0;
    result = rd(tty);
    while (result >= '0' && result <= '9')
    {
      rows = 10 * rows + result - '0';
      result = rd(tty);
    }

    if (result != ';')
      break;

    /* Parse cols. */
    cols = 0;
    result = rd(tty);
    while (result >= '0' && result <= '9')
    {
      cols = 10 * cols + result - '0';
      result = rd(tty);
    }

    if (result != 'R')
      break;

    /* Success! */

    if (rowptr)
      *rowptr = rows;

    if (colptr)
      *colptr = cols;

    retval = 0;

  } while (0);

  /* Restore saved terminal settings. */
  do
  {
    result = tcsetattr(tty, TCSANOW, &saved);
  } while (result == -1 && errno == EINTR);

  if (result == -1 && !retval)
    retval = errno;

  /* Done. */
  close(tty);
  return retval;
}

#endif

Napi::Object Method(const Napi::CallbackInfo &info)
{
  Napi::Env env = info.Env();
  Napi::Object pos = Napi::Object::New(env);

  int row, col;

  row = 0;
  col = 0;

  if (cursor_position(&row, &col))
  {
    return pos;
  }

  if (row < 1 || col < 1)
  {
    return pos;
  }

  pos.Set("row", row);
  pos.Set("col", col);

  return pos;
}

Napi::Object Init(Napi::Env env, Napi::Object exports)
{
  exports.Set(Napi::String::New(env, "sync"), Napi::Function::New(env, Method));
  return exports;
}

NODE_API_MODULE(pos, Init)
