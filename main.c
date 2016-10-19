#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

int main(int argc, char *argv[])
{
  if (argc <= 1) {
    fprintf(stderr, "Usage: %s <gpio>\n", argv[0]);
    return 1;
  }

  char *input = argv[1];
  for (unsigned i = 0; i < strlen(input); ++i) {
    if (isdigit(input[i]) != 0) {
      continue;
    }

    fprintf(stderr, "GPIO pin not numeric. (%s)\n", input);
    return 1;
  }

  char gpio[64];
  snprintf(gpio, sizeof(gpio), "/sys/class/gpio/gpio%s/", input);

  if (access(gpio, F_OK) != 0) {
    fprintf(stderr, "GPIO pin does not appear to be exported. (%s)\n", strerror(errno));
    return 1;
  }

  snprintf(gpio, sizeof(gpio), "/sys/class/gpio/gpio%s/direction", input);

  FILE *directionFile = fopen(gpio, "r");
  if (!directionFile) {
    fprintf(stderr, "Unable to open GPIO direction. (%s)\n", strerror(errno));
    return 1;
  }

  int direction = fgetc(directionFile);
  if (direction == EOF) {
    if (ferror(directionFile) != 0) {
      fprintf(stderr, "Failed to read GPIO direction. (%s)\n", strerror(errno));
    } else {
      fprintf(stderr, "Failed to read GPIO direction. (No data)\n");
    }

    fclose(directionFile);
    return 1;
  }

  fclose(directionFile);

  if (direction != 'i') {
    fprintf(stderr, "GPIO pin is not set to input mode. (%c)\n", direction);
    return 1;
  }

  snprintf(gpio, sizeof(gpio), "/sys/class/gpio/gpio%s/edge", input);

  FILE *edgeFile = fopen(gpio, "r");
  if (!edgeFile) {
    fprintf(stderr, "Unable to open GPIO edge. (%s)\n", strerror(errno));
    return 1;
  }

  int edge = fgetc(edgeFile);
  if (edge == EOF) {
    if (ferror(edgeFile) != 0) {
      fprintf(stderr, "Failed to read GPIO edge. (%s)\n", strerror(errno));
    } else {
      fprintf(stderr, "Failed to read GPIO edge. (No data)\n");
    }

    fclose(edgeFile);
    return 1;
  }

  fclose(edgeFile);

  if (edge != 'r' && edge != 'b') {
    fprintf(stderr, "GPIO pin is not set to detect rising edges. (%c)\n", edge);
    return 1;
  }

  snprintf(gpio, sizeof(gpio), "/sys/class/gpio/gpio%s/value", input);

  FILE *valueFile = fopen(gpio, "r");
  if (!valueFile) {
    fprintf(stderr, "Unable to open GPIO value. (%s)\n", strerror(errno));
    return 1;
  }

  int value = fgetc(valueFile);
  if (value == EOF) {
    if (ferror(valueFile) != 0) {
      fprintf(stderr, "Failed to read GPIO value. (%s)\n", strerror(errno));
    } else {
      fprintf(stderr, "Failed to read GPIO value. (No data)\n");
    }

    fclose(valueFile);
    return 1;
  }

  while (fgetc(valueFile) != EOF) {
    // Do nothing.
  }

  while (1) {
    struct pollfd fdset;
    fdset.fd = fileno(valueFile);
    fdset.events = POLLPRI;

    int pollret = poll(&fdset, 1, -1);

    if (pollret < 0) {
      fprintf(stderr, "Failed to poll for GPIO value. (%s)\n", strerror(errno));
      break;
    }

    fseek(valueFile, 0, SEEK_SET);

    value = fgetc(valueFile);

    while (fgetc(valueFile) != EOF) {
      // Do nothing.
    }

    //fprintf(stdout, "GPIO value: %s\n", (value == '0') ? "low" : "high");
    if (value == '1') {
      fprintf(stdout, ".");
      fflush(stdout);
    }
  }

  fprintf(stdout, "\n");

  fclose(valueFile);

  return 0;
}

