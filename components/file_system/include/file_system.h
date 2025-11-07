#ifndef __FILE_SYSTEM_H__
#define __FILE_SYSTEM_H__

void file_system_init(void);

void file_system_write_string(const char *key, const char *data);
char *file_system_read_string(const char *key);

#endif // __FILE_SYSTEM_H__