#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>

#include <magisk.h>
#include <daemon.h>
#include <flags.h>

#include "magiskhide.h"

using namespace std::literals;

bool hide_enabled = false;

[[noreturn]] static void usage(char *arg0) {
	fprintf(stderr,
		FULL_VER(MagiskHide) "\n\n"
		"Usage: %s [--option [arguments...] ]\n\n"
		"Options:\n"
  		"  --status          Return the status of magiskhide\n"
		"  --enable          Start magiskhide\n"
		"  --disable         Stop magiskhide\n"
		"  --add PKG [PROC]  Add a new target to the hide list\n"
		"  --rm PKG [PROC]   Remove from the hide list\n"
		"  --ls              List the current hide list\n"
#ifdef MAGISK_DEBUG
		"  --test            Run process monitor test\n"
#endif
		, arg0);
	exit(1);
}

void magiskhide_handler(int client) {
	int req = read_int(client);
	int res = DAEMON_ERROR;

	switch (req) {
	case STOP_MAGISKHIDE:
	case ADD_HIDELIST:
	case RM_HIDELIST:
	case LS_HIDELIST:
		if (!hide_enabled) {
			write_int(client, HIDE_NOT_ENABLED);
			close(client);
			return;
		}
	}

	switch (req) {
	case LAUNCH_MAGISKHIDE:
		launch_magiskhide(client);
		return;
	case STOP_MAGISKHIDE:
		res = stop_magiskhide();
		break;
	case ADD_HIDELIST:
		res = add_list(client);
		break;
	case RM_HIDELIST:
		res = rm_list(client);
		break;
	case LS_HIDELIST:
		ls_list(client);
		client = -1;
		break;
	case HIDE_STATUS:
		res = hide_enabled ? HIDE_IS_ENABLED : HIDE_NOT_ENABLED;
		break;
	}

	write_int(client, res);
	close(client);
}

int magiskhide_main(int argc, char *argv[]) {
	if (argc < 2)
		usage(argv[0]);

	int req;
	if (argv[1] == "--enable"sv)
		req = LAUNCH_MAGISKHIDE;
	else if (argv[1] == "--disable"sv)
		req = STOP_MAGISKHIDE;
	else if (argv[1] == "--add"sv)
		req = ADD_HIDELIST;
	else if (argv[1] == "--rm"sv)
		req = RM_HIDELIST;
	else if (argv[1] == "--ls"sv)
		req = LS_HIDELIST;
	else if (argv[1] == "--status"sv)
		req = HIDE_STATUS;
#ifdef MAGISK_DEBUG
	else if (argv[1] == "--test"sv)
		test_proc_monitor();
#endif
	else
		usage(argv[0]);

	// Send request
	int fd = connect_daemon();
	write_int(fd, MAGISKHIDE);
	write_int(fd, req);
	if (req == ADD_HIDELIST || req == RM_HIDELIST) {
		write_string(fd, argv[2]);
		write_string(fd, argv[3] ? argv[3] : "");
	}
	if (req == LS_HIDELIST)
		send_fd(fd, STDOUT_FILENO);

	// Get response
	int code = read_int(fd);
	switch (code) {
	case DAEMON_SUCCESS:
		break;
	case HIDE_NOT_ENABLED:
		fprintf(stderr, "MagiskHide is not enabled\n");
		break;
	case HIDE_IS_ENABLED:
		fprintf(stderr, "MagiskHide is enabled\n");
		break;
	case HIDE_ITEM_EXIST:
		fprintf(stderr, "Target already exists in hide list\n");
		break;
	case HIDE_ITEM_NOT_EXIST:
		fprintf(stderr, "Target does not exist in hide list\n");
		break;
	case HIDE_NO_NS:
		fprintf(stderr, "Your kernel doesn't support mount namespace\n");
		break;

	/* Errors */
	case ROOT_REQUIRED:
		fprintf(stderr, "Root is required for this operation\n");
		break;
	case DAEMON_ERROR:
	default:
		fprintf(stderr, "Daemon error\n");
		return DAEMON_ERROR;
	}

	return req == HIDE_STATUS ? (code == HIDE_IS_ENABLED ? 0 : 1) : code != DAEMON_SUCCESS;
}
