## Synopsis

*acme-client* is yet another
[ACME](https://letsencrypt.github.io/acme-spec/) client, specifically
for [Let's Encrypt](https://letsencrypt.org), but one with a strong
focus on security. 

It was originally named *letskencrypt* until version 0.1.11.

Please see
[kristaps.bsd.lv/acme-client](https://kristaps.bsd.lv/acme-client) for
stable releases: this repository is for current development of the
[OpenBSD](http://www.openbsd.org) version, requiring OpenBSD 5.9 or
greater.  For the portable version (Mac OS X, Linux, FreeBSD, NetBSD,
older OpenBSD) see
[acme-client-portable](https://github.com/kristapsdz/acme-client-portable).

This repository mirrors the master CVS repository: any source changes
will occur in the master and be pushed periodically to GitHub.  If you
have bug reports or patches, either file them here or e-mail them to me.
Feature requests will be ignored unless joined by a patch.

## License

Sources use the ISC (like OpenBSD) license.
See the [LICENSE.md](LICENSE.md) file for details.

The [jsmn.c](jsmn.c) and [jsmn.h](jsmn.h) files use the MIT license.
See [https://github.com/zserge/jsmn](https://github.com/zserge/jsmn) for
details.
