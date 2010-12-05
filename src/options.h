#ifndef OPTIONS_H
#define OPTIONS_H

extern char *progname;

void version(void);
char **parse_options(char **argv);

long option_args_count(void);
char **option_args_setup(char **argv, char *filename);

#endif /* OPTIONS_H */
