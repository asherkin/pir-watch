/**
 * pir-watch.c
 *
 * Publishes messages to a Redis pub/sub channel when motion is detected by a GPIO-attached PIR sensor.
 *
 * Compile:
 *   clang -O3 -Wall -Wextra -pedantic `pkg-config --cflags --libs hiredis` pir-watch.c -o pir-watch
 *
 * Requires a suitible C compiler, pkg-config, and the hiredis library to be installed.
 *
 * Usage:
 *   ./pir-watch 1016 127.0.0.1 6379 sensors.motion.door 1
 *
 * This publishes "1" to a pub/sub channel named "sensors.motion.door" on 127.0.0.1:6379 when GPIO pin 1016 is triggered.
 * The GPIO pin must be exported and setup as an input with rising edge detection before starting pir-watch.
 *
 * Copyright (c) 2016 Asher J.D. Baker
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <hiredis.h>

#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

int is_numeric(const char *input)
{
  for (unsigned i = 0; i < strlen(input); ++i) {
    if (isdigit(input[i]) != 0) {
      continue;
    }

    return 0;
  }

  return 1;
}

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
    }
    else {
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
  signal(SIGPIPE, SIG_IGN);

  if (argc <= 1 || (argc > 2 && argc <= 5)) {
    fprintf(stderr, "Usage: %s <gpio pin> [<redis server> <port> <channel> <message>]\n", argv[0]);
    return 1;
  }

  const char *gpio = argv[1];
  if (is_numeric(gpio) == 0) {
    fprintf(stderr, "GPIO pin not numeric. (%s)\n", gpio);
    return 1;
  }

  const char *redisServer = NULL;
  int redisPort = 0;
  const char *redisChannel = NULL;
  const char *redisMessage = NULL;

  if (argc > 5) {
    const char *redisPortString = argv[3];
    if (is_numeric(redisPortString) == 0) {
      fprintf(stderr, "Redis port not numeric. (%s)\n", redisPortString);
      return 1;
    }

    redisServer = argv[2];
    redisPort = atoi(redisPortString);
    redisChannel = argv[4];
    redisMessage = argv[5];
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
    }
    else {
      fprintf(stderr, "Failed to read GPIO value. (No data)\n");
    }

    fclose(valueFile);
    return 1;
  }

  // Read to EOF to consume all data before we start waiting.
  while (fgetc(valueFile) != EOF) {
    // Do nothing.
  }

  redisContext *redis = NULL;
  if (redisServer) {
    redis = redisConnect(redisServer, redisPort);
    if (!redis || redis->err) {
      if (redis) {
        fprintf(stderr, "Unable to connect to Redis. (%s)\n", redis->errstr);

        redisFree(redis);
      }
      else {
        fprintf(stderr, "Unable to allocate Redis context\n");
      }

      fclose(valueFile);
      return 1;
    }
  }

  int failed = 0;

  while (1) {
    struct pollfd fdset;
    fdset.fd = fileno(valueFile);
    fdset.events = POLLPRI;

    // Wait for the input to change state.
    int pollret = poll(&fdset, 1, -1);

    if (pollret < 0) {
      fprintf(stderr, "Failed to poll for GPIO value. (%s)\n", strerror(errno));
      failed = 1;
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
      if (redis) {
        redisReply *reply = redisCommand(redis, "PUBLISH %s %s", redisChannel, redisMessage);

        if (!reply) {
          fprintf(stderr, "Failed to send PUBLISH command to Redis. (%s)\n", redis->errstr);
          failed = 1;
          break;
        }

        freeReplyObject(reply);
      }

      fprintf(stdout, "%ld motion detected\n", time(NULL));
    }
  }

  if (redis) {
    redisFree(redis);
  }

  fclose(valueFile);

  return failed ? 1 : 0;
}

