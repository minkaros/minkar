# BSD userland

The commands of Minkar's base system: where each comes from, what was
added for macOS compatibility, and what was found and fixed while porting
it. The [roadmap](ROADMAP.md) (Phase 2) tracks this work as a whole;
[Commands.md](Commands.md) counts the image's commands against macOS's.

## Policy

Decision (2026-10-26): the generic Unix commands all come from FreeBSD
`main`. Where macOS's command behaves differently (options, output), our
own code under `__APPLE__` follows macOS's manual page, each difference
with a test. Commands that came from Apple's `shell_cmds` and `adv_cmds`
through ravynOS are replaced. Darwin's own components (launchd, configd,
notifyd, syslogd, the HFS tools, mDNSResponder) stay as they are.
Licences other than BSD and MIT need the project's approval first.

## State

| Group | State |
|---|---|
| [Boot commands](#boot-commands) | Done |
| [Apple's command sets replaced](#apples-command-sets-replaced) | Done; small gaps in `grep` |
| [Everyday tools](#everyday-tools) | In progress: `nslookup`/`dig` and others missing |
| [Power user](#power-user) | `zsh`, `tar`, `ssh`, `curl`, `make` and `git` in the image |
| [Apple tools](#apple-tools) | `open` missing (needs LaunchServices) |
| [System data](#system-data) | Locales, time zones, manual pages, iconv tables |

## Boot commands

`sh`, `ls`, `cp`, `mv`, `rm`, `mount`, `ifconfig`, `ps`, `kill`, tested in
the image on 2026-10-26. `cp -p` failed on every file without an ACL, and
`mv` between volumes warned: Darwin reports "no ACL" as `ENOENT`.

## Apple's command sets replaced

No command from Apple's command sets is left.

| Date | Commands | macOS layer and notes |
|---|---|---|
| 2026-10-26 | `uname` | `-p` prints `i386` |
| 2026-10-26 | `date` | Now in `/bin`; macOS's `mmddHHMM[yy]` order |
| 2026-10-26 | `rm`, `xargs`, `cat`, `env`, `tail`, `test` | |
| 2026-10-26 | `find` | `-xattr`, `-xattrname` |
| 2026-10-26 | `ls` | macOS's `-@`, `-e`, `-O`, `-o` and the `@` marker |
| 2026-10-26 | `id`, `who`, `time` | `id -F` |
| 2026-10-26 | `grep` | Now in `/usr/bin`; `zgrep` is FreeBSD's script. Missing: macOS's `-Z`/`-J` (decompress) |
| 2026-10-27 | `killall` | Now in `/usr/bin`; macOS matches whole names, past `p_comm`'s 16 characters |
| 2026-10-27 | `kill` | In `/bin`, from `sh`'s built-in's source |
| 2026-10-27 | `w` | It had Apple's portions and was not built; macOS's heading |
| 2026-10-27 | `ps` | macOS's displays, options and keywords; process records from libbsdcompat's `kvm_getprocs()` over sysctl and libproc |
| 2026-10-27 | `top` | Our own, with macOS's options, keys and summary; `btop` 1.4.7 (Apache-2.0) next to it |
| 2026-10-27 | `sysctl` | Maps Darwin's OID formats to FreeBSD's types, skips nodes with no format, prints `vm.swapusage` |
| 2026-10-27 | `cal`/`ncal`, `getconf`, `locale` | `getconf` has macOS's `DARWIN_USER_*` names |
| 2026-10-27 | `login`, `su` | `login` is our own (BSD-2-Clause); `su` is FreeBSD main's |

## Everyday tools

| Date | Commands | Notes |
|---|---|---|
| 2026-10-26 | `dd`, `expr`, `printenv`, `od`/`hexdump`, `paste`, `nohup`, `install`, `ed`, `pax` | |
| 2026-10-26 | `pgrep`/`pkill` | FreeBSD's, reading processes through `sysctl` |
| 2026-10-26 | `readlink`, `link`, `unlink`, `egrep`, `fgrep`, `zgrep` | Second names |
| 2026-10-26 | `m4`, `bc`/`dc`, `cpio` | `cpio` is libarchive's |
| 2026-10-26 | `cron`, `crontab` | With a launchd job. Jobs never ran: cron cleared `environ` to NULL, which Darwin's `setenv` cannot start from |
| 2026-10-26 | `rev`, `column`, `comm`, `join`, `fold`, `fmt`, `split`, `csplit`, `tsort`, `nl`, `expand`, `unexpand` | |
| 2026-10-26 | `jot`, `lam`, `rs`, `col`, `colrm`, `look`, `logname`, `renice`, `nice`, `lockf`, `cksum`/`sum`, `what`, `vis`/`unvis` | |
| 2026-10-26 | `mkfifo` | Named pipes failed on HFS+: the hfs kext was built without FIFO support |
| 2026-10-26 | `uuencode`/`uudecode`/`base64` | macOS's `base64` options |
| 2026-10-26 | `xattr` | Written for Minkar, macOS's options; no manual page yet |
| 2026-10-26 | `chflags` | Kernel bug found by its test: a file's owner can unlink it while it is `uchg` |
| 2026-10-27 | `tty` | |
| 2026-10-27 | `man`, `apropos`, `whatis`, `makewhatis` | FreeBSD main's `man.sh`, as on macOS, with `mandoc` 1.14 |
| 2026-10-27 | `units` | `/usr/share/misc/units.lib` is macOS's name for its file |
| 2026-10-27 | `locate` | `locate.updatedb` indexes the local hfs, ufs and apfs volumes (Darwin has no `lsvfs`) |
| 2026-10-27 | `iconv` | FreeBSD main's, on the Citrus libiconv (see [System data](#system-data)) |
| 2026-10-28 | `lsof` | lsof-org 4.99.7 (Purdue licence), its darwin dialect on libproc, in `/usr/sbin`: files, sockets, pipes, `-t`, `-F`, `-J`, `+D`; a user sees only their own processes, as on macOS |
| 2026-10-28 | `zic`, `zdump` | tzcode, in `/usr/sbin` |
| 2026-10-28 | `calendar`, `pr`, `sdiff`, `diff3`, `script`, `whois`, `ul`, `mesg`, `wall`, `write`, `banner`, `compress`/`uncompress`, `gencat`, `cap_mkdb`, `indent`, `ctags` | FreeBSD main's |
| 2026-10-28 | `last` | Reads Darwin's wtmpx log |
| 2026-10-28 | `timeout` | A process group, for want of a reaper |
| 2026-10-28 | `passwd` | macOS's `-i`, `-l`, `-u` and wording; set-user-ID root. pam_unix changes the password with our own code (OpenPAM's could not): a SHA-512 hash in `/etc/master.passwd`, which libinfo reads directly, so FreeBSD's `pw_*` functions and `pwd_mkdb` are not needed |
| 2026-10-28 | `sync`, `mknod`, `nologin`, `asa`, `c99`, `newgrp`, `pagesize` | FreeBSD main's, in macOS's places (`/bin`, `/sbin`, `/usr/bin`) |
| 2026-10-28 | `finger` | The last login from Darwin's lastlog records (`getlastlogxbyname`) |
| 2026-10-28 | `ac` | Reads Darwin's wtmpx log, as `last` does |
| 2026-10-28 | `uuidgen` | Written for Minkar, as on macOS: an upper-case random UUID, `-hdr` for a Core Foundation header line (FreeBSD's is built on the DCE UUID API) |
| 2026-10-28 | `arp`, `ndp`, `traceroute6` | Through the routing socket (FreeBSD's path without netlink). XNU pads sockaddrs to 4 bytes and gives `arp` its expiry in calendar time; `ndp -i` sets flags with Darwin's `SIOCSIFINFO_FLAGS`; `traceroute6 -S` (SCTP) with our own packet headers, as Darwin has none |
| 2026-10-28 | `unzip` | libarchive 3.8.1's bsdunzip (macOS's is Info-ZIP's; `zip` and `zipinfo` are still missing) |
| 2026-10-28 | `at`, `atq`, `atrm`, `batch` | Set-user-ID root; jobs in `/var/at`. `atrun` runs them under launchd every 30 seconds, disabled as on macOS until `launchctl load -w /System/Library/LaunchDaemons/org.minkar.atrun.plist`; launchd leaves its jobs running (`AbandonProcessGroup`) |
| 2026-10-28 | `newsyslog` | Our `/etc/newsyslog.conf` for the daemons' own log files and `/etc/newsyslog.d/`; `system.log` and the ASL store stay syslogd's and aslmanager's, as on macOS. launchd runs it at half past each hour |
| 2026-10-28 | `ipcs`, `ipcrm` | Read XNU's System V IPC tables entry by entry through the `kern.sysv.ipcs.*` sysctls (Darwin has no `kvm(3)`, so `-C`, `-N` and `-y` are refused); `ipcs -S` shows the semaphore limits of `kern.sysv.*`, as the kernel reports what is in use |
| 2026-10-28 | `htop` | htop 3.4.1 (GPL-2.0) on its Darwin backend (libproc, Mach and IOKit), next to `top` and `btop`; not in macOS |

`btop` and `htop` draw their UTF-8 boxes and graphs on the kernel's console
too: it has line characters, blocks and Braille dots.

### Missing and known problems

- `nslookup`, `dig`.
- `getconf DARWIN_USER_DIR` and `DARWIN_USER_CACHE_DIR` fail with EIO (no
  per-user directory service).
- `mtree`: NetBSD's, which FreeBSD builds with its `pack_dev` and libnetbsd.
- `sshd` records a login but not its logout: `last` shows ssh sessions as
  still logged in.
- Until 2026-10-28 an ACL's allow entries never applied (the kernel had no
  GUID for a process without a resolver daemon), so only root could read
  the login records: `last` and `finger` showed nothing to other users.
- The console's terminal has no node in `/dev` (the serial device behind
  it), so `ps` shows `??` for getty.
- The rest of the BSD tools listed in [Commands.md](Commands.md).
- Queued (the kernel already has what they need): FAT's `newfs_msdos` and
  `fsck_msdos` (FreeBSD's) and our own `mount_msdos`, as `msdosfs` is in
  the kernelcache; the quota tools (FreeBSD's, for XNU's quota file
  format); `pfctl` (one that speaks XNU's pf ioctls); `mount_nfs` and
  `nfsd` (XNU has the NFS client and server).
- Left out, with no driver under a usable license: exFAT
  (`fsck_exfat`; the free tools are GPL), SMB (`mount_smbfs`; Samba is
  GPL and FreeBSD's smbfs is gone) and APFS (`fsck_apfs`, `mount_apfs`,
  `newfs_apfs`; no open implementation). Minkar's file system is HFS+.

## Power user

- `tar`, `ssh` and `curl` are installed.
- `zsh` 5.9, built and tested 2026-10-26: modules linked in, the function
  library, listed in `/etc/shells`.
- `make` is BSD make (bmake 20260704, with its mk files in `/usr/share/mk`)
  since 2026-10-27, chosen over GNU make.
- `git` 2.56.0 (GPL-2.0, approved 2026-10-27) since 2026-10-28, built by its
  own Makefile with its Darwin settings: HTTPS through libcurl,
  `git daemon`, accented file names on HFS Plus (core.precomposeunicode),
  the manuals and completion. No Perl, Python or Tcl commands (`git svn`,
  `send-email`, `gitk`, `git gui`).

## Apple tools

- `plutil`, `defaults`, `sw_vers` and `ditto`, tested in the image
  2026-10-26 (`sw_vers` printed spaces instead of macOS's tabs and a
  trailing blank line).
- `chmod` has macOS's ACL syntax (`+a`, `-a`, `=a#`, `-E`, `-C`, `-N`, `-i`,
  `-I`) since 2026-10-26, and the kernel enforces the entries.
- `open` is missing: it needs LaunchServices. `hdiutil` comes with Phase 6.

## System data

- **Locales** (2026-10-27): 56 UTF-8 locales in `/usr/share/locale`, made
  by `tools/locale/mklocales.py` in the formats Darwin's libc reads:
  character classes, case maps and widths from Unicode 16; time, money,
  numbers and messages from FreeBSD's CLDR definitions; collation by code
  point.
- **Time zones** (2026-10-27): IANA tzdata 2026e compiled by tzcode's
  `zic`, laid out as on macOS (`/var/db/timezone/zoneinfo`). Until then
  every zone was UTC.
- **Manual pages** (2026-10-27): the pages the commands' sources carry;
  every command has one, or is listed as still without.
- **libiconv** (2026-10-27): FreeBSD's Citrus, as on macOS 14. The GNU one
  before exported only `libiconv_open`, not `iconv_open`, and the SDK had
  no `<iconv.h>`. `/usr/lib/libiconv.2.dylib` has its 23 encoding modules
  linked in, with 687 conversion tables in `/usr/share/i18n`; CoreText's
  libxml2 and fontconfig use it. UTF-8-MAC (2026-10-28, ours, from the
  Unicode data): the decomposed names HFS Plus stores, composed again for
  git.
