/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2024 Georgy Dyuldin aka R2RFE
 */

#include "db.h"

#include "migrations.h"
#include "../util.h"

#include <string.h>
#include <lvgl/src/misc/lv_log.h>


sqlite3                 *db = NULL;

static sqlite3_stmt     *insert_stmt;


/* A params.db without bands (an old or broken one kept on the DATA partition when the
 * SD card was written again): the bands of the default database, the other settings kept */
#define DEFAULT_DB "/usr/share/x6100/params.default.db"

static void bands_check(void) {
    sqlite3_stmt *st;
    int           n = 0;

    if (sqlite3_prepare_v2(db, "SELECT count(*) FROM bands", -1, &st, 0) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) {
            n = sqlite3_column_int(st, 0);
        }
        sqlite3_finalize(st);
    }
    if (n > 0) {
        return;
    }
    LV_LOG_WARN("params.db has no bands: loading the default ones");
    if (sqlite3_exec(db,
            "ATTACH '" DEFAULT_DB "' AS def;"
            "CREATE TABLE IF NOT EXISTS bands(id INTEGER PRIMARY KEY, name TEXT, start_freq INTEGER, "
                "stop_freq INTEGER, type INTEGER);"
            "CREATE TABLE IF NOT EXISTS band_params(bands_id INTEGER, name TEXT, val INTEGER, "
                "UNIQUE (bands_id, name) ON CONFLICT REPLACE);"
            "BEGIN;"
            "DELETE FROM band_params;"
            "INSERT INTO bands SELECT * FROM def.bands;"
            "INSERT INTO band_params SELECT * FROM def.band_params;"
            "COMMIT;", NULL, NULL, NULL) != SQLITE_OK) {
        LV_LOG_ERROR("Can't load the default bands: %s", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
    }
    sqlite3_exec(db, "DETACH def;", NULL, NULL, NULL);
}

static void errorLogCallback(void *pArg, int iErrCode, const char *zMsg){
    LV_LOG_ERROR("(%d) %s\n", iErrCode, zMsg);
}

bool database_init() {
    sqlite3_config(SQLITE_CONFIG_LOG, errorLogCallback, NULL);
    sqlite3_config(SQLITE_CONFIG_SERIALIZED);

    int rc = sqlite3_open("/mnt/params.db", &db);

    if (rc != SQLITE_OK) {
        LV_LOG_ERROR("Can't open params.db");
        return false;
    }

    rc = migrations_apply();
    if (rc != 0) {
        return false;
    }
    bands_check();

    // Some optimizations
    //
    sqlite3_exec(db, "PRAGMA synchronous = OFF;", NULL, NULL, NULL);
    sqlite3_exec(db, "PRAGMA journal_mode = WAL;", NULL, NULL, NULL);
    sqlite3_exec(db, "PRAGMA cache_size = -4096;", NULL, NULL, NULL);


    rc = sqlite3_prepare_v2(db, "INSERT INTO params(name, val) VALUES(?, ?)", -1, &insert_stmt, 0);

    if (rc != SQLITE_OK) {
        LV_LOG_ERROR("Can't prepare insert statement for params");
        return false;
    }
    return true;
}

bool sql_query_exec(const char *sql) {
    char    *err = 0;
    int     rc;

    rc = sqlite3_exec(db, sql, NULL, NULL, &err);

    if (rc != SQLITE_OK) {
        LV_LOG_ERROR(err);
        return false;
    }
    return true;
}


void params_write_int(const char *name, int data, bool *dirty) {
    sqlite3_bind_text(insert_stmt, 1, name, strlen(name), 0);
    sqlite3_bind_int(insert_stmt, 2, data);
    sqlite3_step(insert_stmt);
    sqlite3_reset(insert_stmt);
    sqlite3_clear_bindings(insert_stmt);

    *dirty = false;
}

void params_write_float(const char *name, float data, bool *dirty) {
    sqlite3_bind_text(insert_stmt, 1, name, strlen(name), 0);
    sqlite3_bind_double(insert_stmt, 2, (double) data);
    sqlite3_step(insert_stmt);
    sqlite3_reset(insert_stmt);
    sqlite3_clear_bindings(insert_stmt);

    *dirty = false;
}

void params_write_text(const char *name, const char *data, bool *dirty) {
    sqlite3_bind_text(insert_stmt, 1, name, strlen(name), 0);
    sqlite3_bind_text(insert_stmt, 2, data, strlen(data), 0);
    sqlite3_step(insert_stmt);
    sqlite3_reset(insert_stmt);
    sqlite3_clear_bindings(insert_stmt);

    *dirty = false;
}
