#ifndef REPLAY_H
#define REPLAY_H

/* Attempts to run a program with all system calls replaced
 * by those recorded in a record phase
 */
void replay(int read_fd, int verbosity);

#endif /* REPLAY_H */
