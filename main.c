#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int gpio_get_config(const char *gpio, const char *config)
{
  char filename[64];
  snprintf(filename, sizeof(filename), "/sys/class/gpio/gpio%s/%s", gpio, config);

  FILE *configFile = fopen(filename, "r");
  if (!configFile) {
    fprintf(stderr, "Unable to open GPIO %s config. (%s)\n", config, strerror(errno));
    return EOF;
  }

  int value = fgetc(configFile);
  if (value == EOF) {
    if (ferror(configFile) != 0) {
      fprintf(stderr, "Failed to read GPIO %s config. (%s)\n", config, strerror(errno));
    } else {
      fprintf(stderr, "Failed to read GPIO %s config. (No data)\n", config);
    }

    fclose(configFile);
    return EOF;
  }

  fclose(configFile);

  return value;
}

int main(int argc, char *argv[])
{
  if (argc <= 1) {
    fprintf(stderr, "Usage: %s <gpio>\n", argv[0]);
    return 1;
  }

  char *gpio = argv[1];
  for (unsigned i = 0; i < strlen(gpio); ++i) {
    if (isdigit(gpio[i]) != 0) {
      continue;
    }

    fprintf(stderr, "GPIO pin not numeric. (%s)\n", gpio);
    return 1;
  }

  char filename[64];
  snprintf(filename, sizeof(filename), "/sys/class/gpio/gpio%s/", gpio);

  if (access(filename, F_OK) != 0) {
    fprintf(stderr, "GPIO pin does not appear to be exported. (%s)\n", strerror(errno));
    return 1;
  }

  int direction = gpio_get_config(gpio, "direction");
  if (direction != 'i') {
    fprintf(stderr, "GPIO pin is not set to input mode. (%c)\n", direction);
    return 1;
  }

  int edge = gpio_get_config(gpio, "edge");
  if (edge != 'r' && edge != 'b') {
    fprintf(stderr, "GPIO pin is not set to detect rising edges. (%c)\n", edge);
    return 1;
  }

  snprintf(filename, sizeof(filename), "/sys/class/gpio/gpio%s/value", gpio);

  FILE *valueFile = fopen(filename, "r");
  if (!valueFile) {
    fprintf(stderr, "Unable to open GPIO value. (%s)\n", strerror(errno));
    return 1;
  }

  // Check that we get a sane value for the input before starting.
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

  // Read to EOF to consume all data before we start waiting.
  while (fgetc(valueFile) != EOF) {
    // Do nothing.
  }

  while (1) {
    struct pollfd fdset;
    fdset.fd = fileno(valueFile);
    fdset.events = POLLPRI;

    // Wait for the input to change state.
    int pollret = poll(&fdset, 1, -1);

    if (pollret < 0) {
      fprintf(stderr, "Failed to poll for GPIO value. (%s)\n", strerror(errno));
      break;
    }

    // Read the input value.
    fseek(valueFile, 0, SEEK_SET);
    value = fgetc(valueFile);

    // Read to EOF to clear the poll queue.
    while (fgetc(valueFile) != EOF) {
      // Do nothing.
    }

    // If the input is high, we've detected motion.
    if (value == '1') {
      fprintf(stdout, ".");
      fflush(stdout);
    }
  }

  fprintf(stdout, "\n");

  fclose(valueFile);

  return 0;
}

