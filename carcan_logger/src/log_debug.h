#ifndef __LOG_DEBUG__
#define __LOG_DEBUG__

#define DEBUG 1
extern unsigned long cmillis;
#define DBUFMAX 255
static char dbuff[DBUFMAX];
#define log_print(...)                           \
  do                                             \
  {                                              \
    if (DEBUG > 0)                               \
    {                                            \
      snprintf(dbuff, DBUFMAX - 1, __VA_ARGS__); \
      Serial.print(dbuff);                       \
    }                                            \
  } while (0)
#define log_info1(...)                                   \
  do                                                     \
  {                                                      \
    if (DEBUG > 0)                                       \
    {                                                    \
      snprintf(dbuff, DBUFMAX - 1, "#[%lu]i:", cmillis); \
      Serial.print(dbuff);                               \
      snprintf(dbuff, DBUFMAX - 1, __VA_ARGS__);         \
      Serial.println(dbuff);                             \
    }                                                    \
  } while (0)
#define log_warn1(...)                                   \
  do                                                     \
  {                                                      \
    if (DEBUG > 0)                                       \
    {                                                    \
      snprintf(dbuff, DBUFMAX - 1, "#[%lu]w:", cmillis); \
      Serial.print(dbuff);                               \
      snprintf(dbuff, DBUFMAX - 1, __VA_ARGS__);         \
      Serial.println(dbuff);                             \
    }                                                    \
  } while (0)
#define log_err1(...)                                    \
  do                                                     \
  {                                                      \
    if (DEBUG > 0)                                       \
    {                                                    \
      snprintf(dbuff, DBUFMAX - 1, "#[%lu]e:", cmillis); \
      Serial.print(dbuff);                               \
      snprintf(dbuff, DBUFMAX - 1, __VA_ARGS__);         \
      Serial.println(dbuff);                             \
    }                                                    \
  } while (0)

#endif
