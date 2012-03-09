#ifndef UTILS_H
#define UTILS_H

char *utils_strsep(char **stringp, const char *delim);
int utils_read_file(char **dst, const char *pemstr);
int utils_hwaddr_raop(char *str, int strlen, const char *hwaddr, int hwaddrlen);
int utils_hwaddr_airplay(char *str, int strlen, const char *hwaddr, int hwaddrlen);

#endif
