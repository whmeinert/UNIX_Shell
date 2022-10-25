
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "dispatcher.h"
#include "shell_builtins.h"
#include "parser.h"

static int execArgs(struct command *pipeline);
static int execArgsPiped(struct command *pipeline);
static int dispatch_external_command(struct command *pipeline);


static int execArgsPiped(struct command *pipeline)
{	
	int status = 0;
	
	while (pipeline->pipe_to != NULL){
		int pipefd[2];
		int child;

		pipe(pipefd);
		child = fork();

		if(child < 0) {
			perror("error: could not fork");
			exit(-1);
		}

		if (child == 0) {
			dup2(pipefd[1],1);
			close(pipefd[0]);
			status = execArgs(pipeline);
			exit(0);
		} else {
			wait(&status);

			dup2(pipefd[0],0);
			close(pipefd[1]);
			status = dispatch_external_command(pipeline->pipe_to);
		}
		exit(status);
	}
	return status;
}


static int execArgs(struct command *pipeline)
{
	/* initializing status */
	int status = 0;

	/* Forking a child */
	pid_t pid = fork();

	/* check if fork failed */
	if (pid == -1) {
		fprintf(stderr, "error: failed forking child\n");
		return -1;
	} else if (pid == 0) {  // child process

		if (pipeline->input_filename != NULL) {
			int fd0;
			if ((fd0 = open(pipeline->input_filename, O_RDONLY)) < 0) {
				fprintf(stderr, "error: couldn't open the input file\n");
				exit(-1);
			}
			dup2(fd0, STDIN_FILENO);
				close(fd0);
		}

		if (pipeline->output_type == COMMAND_OUTPUT_FILE_APPEND){
			int fd1 ;
			if ((fd1 = open(pipeline->output_filename, O_WRONLY | O_APPEND | O_CREAT, 0644)) < 0) {
				fprintf(stderr, "error: couldn't open the output file\n");
				exit(-1);
			}

			dup2(fd1, STDOUT_FILENO);
			close(fd1);
		}

		if (pipeline->output_type == COMMAND_OUTPUT_FILE_TRUNCATE){
			int fd2;
			if ((fd2 = creat(pipeline->output_filename, 0644)) < 0) {
				fprintf(stderr, "error: couldn't open the output file\n");
				exit(-1);
			}

			dup2(fd2, STDOUT_FILENO);
			close(fd2);
		}

		/* Check if the command exists and if it doesnt exit*/
		if (execvp(pipeline->argv[0], pipeline->argv) < 0) {
			fprintf(stderr, "error: Command not found\n");
			exit(-1);
		}
		exit(0);
	} else {
		/* waiting for child to terminate */
		wait(&status);

		/* Check exit status and return accordingly */
	}
	if (WEXITSTATUS(status) != 0)
		return -1;  // return if error
	else
		return 0;  // return if no error
}

/**
 * dispatch_external_command() - run a pipeline of commands
 *
 * @pipeline:   A "struct command" pointer representing one or more
 *              commands chained together in a pipeline.  See the
 *              documentation in parser.h for the layout of this data
 *              structure.  It is also recommended that you use the
 *              "parseview" demo program included in this project to
 *              observe the layout of this structure for a variety of
 *              inputs.
 *
 * Note: this function should not return until all commands in the
 * pipeline have completed their execution.
 *
 * Return: The return status of the last command executed in the
 * pipeline.
 */
static int dispatch_external_command(struct command *pipeline)
{
	/*
	 * Note: this is where you'll start implementing the project.
	 *
	 * It's the only function with a "TODO".  However, if you try
	 * and squeeze your entire external command logic into a
	 * single routine with no helper functions, you'll quickly
	 * find your code becomes sloppy and unmaintainable.
	 *
	 * It's up to *you* to structure your software cleanly.  Write
	 * plenty of helper functions, and even start making yourself
	 * new files if you need.
	 *
	 * For D1: you only need to support running a single command
	 * (not a chain of commands in a pipeline), with no input or
	 * output files (output to stdout only).  In other words, you
	 * may live with the assumption that the "input_file" field in
	 * the pipeline struct you are given is NULL, and that
	 * "output_type" will always be COMMAND_OUTPUT_STDOUT.
	 *
	 * For D2: you'll extend this function to support input and
	 * output files, as well as pipeline functionality.
	 *
	 * Good luck!
	 */
	 
	int status;
	if (pipeline->output_type != COMMAND_OUTPUT_PIPE){
		return execArgs(pipeline);
	}

	else {
		pid_t child = fork();	
		if (child==0) {
			execArgsPiped(pipeline);
			exit(child);
		} else {
			wait(&status);
		}
		return WEXITSTATUS(status);
	}
}




/**
 * dispatch_parsed_command() - run a command after it has been parsed
 *
 * @cmd:                The parsed command.
 * @last_rv:            The return code of the previously executed
 *                      command.
 * @shell_should_exit:  Output parameter which is set to true when the
 *                      shell is intended to exit.
 *
 * Return: the return status of the command.
 */
static int dispatch_parsed_command(struct command *cmd, int last_rv,
				   bool *shell_should_exit)
{
	/* First, try to see if it's a builtin. */
	for (size_t i = 0; builtin_commands[i].name; i++) {
		if (!strcmp(builtin_commands[i].name, cmd->argv[0])) {
			/* We found a match!  Run it. */
			return builtin_commands[i].handler(
				(const char *const *)cmd->argv, last_rv,
				shell_should_exit);
		}
	}

	/* Otherwise, it's an external command. */
	return dispatch_external_command(cmd);
}

int shell_command_dispatcher(const char *input, int last_rv,
			     bool *shell_should_exit)
{
	int rv;
	struct command *parse_result;
	enum parse_error parse_error = parse_input(input, &parse_result);

	if (parse_error) {
		fprintf(stderr, "Input parse error: %s\n",
			parse_error_str[parse_error]);
		return -1;
	}

	/* Empty line */
	if (!parse_result)
		return last_rv;

	rv = dispatch_parsed_command(parse_result, last_rv, shell_should_exit);
	free_parse_result(parse_result);
	return rv;
}
