
#ifndef Header_systemutil
#define Header_systemutil

/* copypaste https://stackoverflow.com/questions/5919996/how-to-detect-reliably-mac-os-x-ios-linux-windows-in-c-preprocessor */
#if defined WIN32 || defined _WIN32 || defined __WIN32__ || defined __NT__
#	define OS_WINDOWS 1
#	ifdef _WIN64
#		define OS_WINDOWS_64 1
#	else
#		define OS_WINDOWS_32 1
#	endif
#elif __APPLE__
#	define OS_APPLE 1
#	include <TargetConditionals.h>
#	if TARGET_IPHONE_SIMULATOR

#	elif TARGET_OS_MACCATALYST

#	elif TARGET_OS_IPHONE

#	elif TARGET_OS_MAC

#	else
#		error "Unknown Apple platform"
#	endif
#elif __linux__
#	define OS_LINUX 1
#elif __unix__
#	define OS_UNIX 1
#elif defined _POSIX_VERSION
#	define OS_POSIX 1
#else
#   error "Unknown compiler"
#endif

enum access_mode {
	Access_Mode_read = 0x1,
	Access_Mode_write = 0x2,
	Access_Mode_execute = 0x4,
	Access_Mode_exists = 0x8,
};

int		check_file_access (const char *path, enum access_mode);
char	*read_output_of_program (const char *program_path, int read_from_fd, int args_count, char *args[], char *env[]);
int		execute_program (const char *program_path, int read_from_fd, const char *input, usize input_size, char **poutput, int args_count, char *args[], char *env[]);


#endif /* Header_systemutil */

#if (defined Implementation_systemutil || defined Implementation_All) && !defined Except_Implementation_systemutil && !defined Implemented_systemutil
#define Implemented_systemutil

#if defined OS_LINUX || defined OS_APPLE || defined OS_UNIX
#	include <unistd.h>
#else
#	error There is no implementation of systemutil for this OS
#endif

#include "def.h"

int		check_file_access (const char *path, enum access_mode access_mode) {
	int		mode;

	mode = 0;
	mode |= (access_mode & Access_Mode_read) ? R_OK : 0;
	mode |= (access_mode & Access_Mode_write) ? W_OK : 0;
	mode |= (access_mode & Access_Mode_execute) ? X_OK : 0;
	mode |= (access_mode & Access_Mode_exists) ? F_OK : 0;
	return (0 == access (path, mode));
}

char	*read_output_of_program (const char *program_path, int read_from_fd, int args_count, char *args[], char *env[]) {
	char	*output = 0;
	int		exit_code;

	if (0 != (exit_code = execute_program (program_path, read_from_fd, 0, 0, &output, args_count, args, env))) {
		Error ("program existed with error %d", exit_code);
	}
	return (output);
}

int		execute_program (const char *program_path, int read_from_fd, const char *input, usize input_size, char **poutput, int args_count, char *args[], char *env[]) {
	int		outpipes[2], inpipes[2], outerrpipes[2];
	char	*output = 0;
	int		exit_code = -1;

	if (0 == pipe (outpipes)) {
		if (0 == pipe (inpipes)) {
			if (0 == pipe (outerrpipes)) {
				int fork_result = fork ();

				if (fork_result == 0) {
					close (outpipes[0]);
					close (outerrpipes[0]);
					close (inpipes[1]);
					if (0 > dup2 (inpipes[0], 0)) {
						Error ("cannot dup2 for input");
						perror (__func__);
						exit (1);
					}
					if (0 > dup2 (outpipes[1], 1)) {
						Error ("cannot dup2 for output");
						perror (__func__);
						exit (1);
					}
					if (0 > dup2 (outerrpipes[1], 2)) {
						Error ("cannot dup2 for error output");
						perror (__func__);
						exit (1);
					}
					execve (program_path, args, env);
					Error ("cannot execve");
					perror (__func__);
					exit (1);
				} else if (fork_result < 0) {
					Error ("cannot fork");
					perror (__func__);
				} else {
					char	*array = 0;
					void	*memory;
					usize	size = 0;
					usize	readed = 0, written = 0;
					int		success = 1;
					int		status;
					int		silent_read_from;

					close (outpipes[1]);
					close (outerrpipes[1]);
					if (input_size <= 0) {
						close (inpipes[1]);
					}
					if (read_from_fd == 1) {
						read_from_fd = outpipes[0];
						silent_read_from = outerrpipes[0];
					} else {
						read_from_fd = outerrpipes[0];
						silent_read_from = outpipes[0];
					}
					memory = expand_array (array, &size);
					if (memory) {
						ssize_t	just_readed, just_written;
						int		is_end = 0;
						char	silent_buffer[256];

						while (success && !is_end) {
							if (success && input_size - written > 0 && (just_written = write (inpipes[1], input + written, input_size - written)) > 0) {
								written += just_written;
							}
							array = memory;
							if (success && size - readed > 0 && (just_readed = read (read_from_fd, array + readed, size - readed - 1)) > 0) {
								readed += just_readed;
								if (size == readed + 1) {
									memory = expand_array (array, &size);
									if (memory) {
										array = memory;
										array[readed] = 0;
									} else {
										success = 0;
									}
								} else {
									array[readed] = 0;
								}
							}
							if (just_readed < 0) {
								Error ("cannot read from pipe");
								perror (__func__);
								success = 0;
							} else if (just_readed == 0) {
								is_end = 1;
							}
							read (silent_read_from, silent_buffer, sizeof silent_buffer);
						}
					} else {
						success = 0;
					}
					close (outpipes[0]);
					close (outerrpipes[0]);
					if (input_size > 0) {
						close (inpipes[1]);
					}
					if (!success && array) {
						free (array);
						array = 0;
					}
					if (0 > waitpid (fork_result, &status, 0)) {
						Error ("cannot waitpid");
						perror (__func__);
						free (array);
						array = 0;
					}
					if (WIFEXITED (status)) {
						exit_code = WEXITSTATUS (status);
					}
					output = array;
				}
			} else {
				Error ("cannot create pipes");
				perror (__func__);
				close (outpipes[0]);
				close (outpipes[1]);
				close (inpipes[0]);
				close (inpipes[1]);
			}
		} else {
			Error ("cannot create pipes");
			perror (__func__);
			close (outpipes[0]);
			close (outpipes[1]);
		}
	} else {
		Error ("cannot create pipes");
		perror (__func__);
	}
	*poutput = output;
	return (exit_code);
}


#endif /* (defined Implementation_systemutil || defined Implementation_All) && !defined Except_Implementation_systemutil && !defined Implemented_systemutil */
