/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * SQLite's private file controls: truncating or replacing a damaged
 * database is not something SQLite has (the call fails and authd makes
 * the database again).
 */

#ifndef MINKAR_AUTHD_SQLITE3_PRIVATE_H
#define MINKAR_AUTHD_SQLITE3_PRIVATE_H

#include <sqlite3.h>

#define SQLITE_TRUNCATE_DATABASE		101
#define SQLITE_REPLACE_DATABASE			102
#define SQLITE_TRUNCATE_JOURNALMODE_WAL		(0x1 << 0)
#define SQLITE_TRUNCATE_AUTOVACUUM_FULL		(0x1 << 4)

#endif
