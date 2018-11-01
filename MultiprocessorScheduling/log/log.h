#ifndef LOG_H
#define LOG_H

#define LOG_CONSOL "consol"

// If you want to log message to a file specified by you,
// Please give a file name.
// If the file name is NULL,
// would log message on console automatically.
int log_open(char* file_name);

// Close the log function.
int log_close();

// Log message into the file you specified.
int log_c(const char* fmt,...);

// For logging message immediatelly,
// If file name is NULL, it would log message on console.
int log_once(char* file_name,const char* fmt,...);

#endif