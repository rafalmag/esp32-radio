// from http://forum.arduino.cc/index.php?topic=64555.0

//#define SERIAL_DEBUG_ENABLED 1

#define GET_NUM_ARGS(...) GET_NUM_ARGS_ACT(__VA_ARGS__, 5, 4, 3, 2, 1)
#define GET_NUM_ARGS_ACT(_1, _2, _3, _4, _5, N, ...) N

#define macro_dispatcher(func, ...) \
      macro_dispatcher_(func, GET_NUM_ARGS(__VA_ARGS__))
#define macro_dispatcher_(func, nargs) \
      macro_dispatcher__(func, nargs)
#define macro_dispatcher__(func, nargs) \
      func##nargs

#ifdef SERIAL_DEBUG_ENABLED
#define DebugPrint(...) macro_dispatcher(DebugPrint, __VA_ARGS__)(__VA_ARGS__)
#define DebugPrintln(...) macro_dispatcher(DebugPrintln, __VA_ARGS__)(__VA_ARGS__)
#define DebugPrint2(str, modifier)       \
      Serial.print(millis());            \
      Serial.print(": ");                \
      Serial.print(__PRETTY_FUNCTION__); \
      Serial.print(' ');                 \
      Serial.print(__LINE__);            \
      Serial.print(' ');                 \
      Serial.print(str, modifier);
#define DebugPrint1(str)                 \
      Serial.print(millis());            \
      Serial.print(": ");                \
      Serial.print(__PRETTY_FUNCTION__); \
      Serial.print(' ');                 \
      Serial.print(__LINE__);            \
      Serial.print(' ');                 \
      Serial.print(str);
#define DebugPrintln2(str, modifier)     \
      Serial.print(millis());            \
      Serial.print(": ");                \
      Serial.print(__PRETTY_FUNCTION__); \
      Serial.print(' ');                 \
      Serial.print(__LINE__);            \
      Serial.print(' ');                 \
      Serial.println(str, modifier);
#define DebugPrintln1(str)               \
      Serial.print(millis());            \
      Serial.print(": ");                \
      Serial.print(__PRETTY_FUNCTION__); \
      Serial.print(' ');                 \
      Serial.print(__LINE__);            \
      Serial.print(' ');                 \
      Serial.println(str);
#else
#define DebugPrint(...) macro_dispatcher(DebugPrint, __VA_ARGS__)(__VA_ARGS__)
#define DebugPrintln(...) macro_dispatcher(DebugPrintln, __VA_ARGS__)(__VA_ARGS__)
#define DebugPrint1(str)
#define DebugPrintln1(str)
#define DebugPrint2(str, modifier)
#define DebugPrintln2(str, modifier)
#endif