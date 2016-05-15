/*	$Id$ */
/*
 * Copyright (c) 2016 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/wait.h>

#include <errno.h>
#ifdef __linux__
# include <grp.h>
#endif
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

static	volatile sig_atomic_t sig;

static	const char *const comms[COMM__MAX] = {
	"req", /* COMM_REQ */
	"thumbprint", /* COMM_THUMB */
	"cert", /* COMM_CERT */
	"payload", /* COMM_PAY */
	"nonce", /* COMM_NONCE */
	"token", /* COMM_TOK */
	"challenge-op", /* COMM_CHNG_OP */
	"challenge-ack", /* COMM_CHNG_ACK */
	"account", /* COMM_SIGN */
	"csr", /* COMM_CSR */
	"issuer", /* COMM_ISSUER */
	"chain", /* COMM_CHAIN */
};

static void
sigpipe(int code)
{

	(void)code;
	sig = 1;
}

/*
 * This will read a long-sized operation.
 * Operations are usually enums, so this should be alright.
 * We return 0 on EOF and LONG_MAX on failure.
 */
long
readop(int fd, enum comm comm)
{
	ssize_t	 	 ssz;
	long		 op;

	ssz = read(fd, &op, sizeof(long));
	if (ssz < 0) {
		dowarn("read: %s", comms[comm]);
		return(LONG_MAX);
	} else if (ssz && ssz != sizeof(long)) {
		dowarnx("short read: %s", comms[comm]);
		return(LONG_MAX);
	} else if (0 == ssz)
		return(0);

	return(op);
}

char *
readstr(int fd, enum comm comm)
{
	size_t	 sz;

	return(readbuf(fd, comm, &sz));
}

char *
readbuf(int fd, enum comm comm, size_t *sz)
{
	ssize_t		 ssz;
	char		*p;

	p = NULL;

	if ((ssz = read(fd, sz, sizeof(size_t))) < 0)
		dowarn("read: %s length", comms[comm]);
	else if ((size_t)ssz != sizeof(size_t))
		dowarnx("short read: %s length", comms[comm]);
	else if (NULL == (p = calloc(1, *sz + 1)))
		dowarn("malloc");
	else if ((ssz = read(fd, p, *sz)) < 0)
		dowarn("read: %s", comms[comm]);
	else if ((size_t)ssz != *sz)
		dowarnx("short read: %s", comms[comm]);
	else
		return(p);

	free(p);
	return(NULL);
}

/*
 * Wring a long-value to a communication pipe.
 * Returns zero if the write failed or the pipe is not open, otherwise
 * return non-zero.
 */
int
writeop(int fd, enum comm comm, long op)
{
	void	(*sig)(int);
	ssize_t	 ssz;
	int	 rc;

	rc = 0;
	/* Catch a closed pipe. */
	sig = signal(SIGPIPE, sigpipe);

	if ((ssz = write(fd, &op, sizeof(long))) < 0) 
		dowarn("write: %s", comms[comm]);
	else if ((size_t)ssz != sizeof(long))
		dowarnx("short write: %s", comms[comm]);
	else
		rc = 1;

	/* Reinstate signal handler. */
	signal(SIGPIPE, sig);
	sig = 0;
	return(rc);
}

int
writebuf(int fd, enum comm comm, const void *v, size_t sz)
{
	ssize_t	 ssz;
	int	 rc;
	void	(*sig)(int);

	rc = 0;
	/* Catch a closed pipe. */
	sig = signal(SIGPIPE, sigpipe);

	if ((ssz = write(fd, &sz, sizeof(size_t))) < 0) 
		dowarn("write: %s length", comms[comm]);
	else if ((size_t)ssz != sizeof(size_t))
		dowarnx("short write: %s length", comms[comm]);
	else if ((ssz = write(fd, v, sz)) < 0)
		dowarn("write: %s", comms[comm]);
	else if ((size_t)ssz != sz)
		dowarnx("short write: %s", comms[comm]);
	else
		rc = 1;

	/* Reinstate signal handler. */
	signal(SIGPIPE, sig);
	sig = 0;
	return(rc);
}

int
writestr(int fd, enum comm comm, const char *v)
{

	return(writebuf(fd, comm, v, strlen(v)));
}

/*
 * Make sure that the given process exits properly.
 */
int
checkexit(pid_t pid, enum comp comp)
{
	int	 c;

	if (-1 == waitpid(pid, &c, 0)) {
		dowarn("waitpid");
		return(0);
	}

	if ( ! WIFEXITED(c)) 
		dowarnx("bad exit: %s(%u) (%s)", 
			compname(comp), pid,
			WIFSIGNALED(c) ?
			sys_signame[WTERMSIG(c)] :
			"not-a-signal");
	else if (EXIT_SUCCESS != WEXITSTATUS(c))
		dowarnx("bad exit code: %s(%u)", compname(comp), pid);
	else
		return(1);

	return(0);
}

/*
 * Safely chroot() into the desired directory.
 * Returns zero on failure, non-zero on success.
 */
int
dropfs(const char *root)
{

	if (-1 == chroot(root))
		dowarn("%s: chroot", root);
	else if (-1 == chdir("/")) 
		dowarn("/: chdir");
	else
		return(1);

	return(0);
}

/*
 * Safely drop privileges into the given credentials.
 * Returns zero on failure, non-zero on success.
 */
int
dropprivs(uid_t uid, gid_t gid)
{

#if defined(__OpenBSD__)
	if (setgroups(1, &gid) ||
	    setresgid(gid, gid, gid) ||
	    setresuid(uid, uid, uid)) {
		dowarn("drop privileges");
		return(0);
	}
#else
	if (setgroups(1, &gid) ||
	    setegid(gid) || setgid(gid) ||
	    seteuid(uid) || setuid(uid)) {
		dowarn("drop privileges");
		return(0);
	}
#endif

	if (getgid() != gid || getegid() != gid) {
		dowarnx("failed to drop gid");
		return(0);
	}
	if (getuid() != uid || geteuid() != uid) {
		dowarnx("failed to drop uid");
		return(0);
	}

	return(1);
}
