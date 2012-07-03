
#include "config.h"

#define TRUE  1
#define FALSE 0

void ml_sqlite3_raise_exn (int, const char *, int) Noreturn;
#define raise_sqlite3_exn(db)	ml_sqlite3_raise_exn (sqlite3_errcode (Sqlite3_val(db)), sqlite3_errmsg (Sqlite3_val(db)), TRUE)


#if defined(__GNUC__) && (__GNUC__ >= 3)
# define Pure	__attribute__ ((pure))
#else
# define Pure
#endif

struct ml_sqlite3_data {
  sqlite3 *db;
  value  callbacks;
  value  stmt_store;
};

#define Sqlite3_data_val(v)	(* ((struct ml_sqlite3_data **) Data_custom_val(v)))

static sqlite3 *	Sqlite3_val       (value) Pure;
static sqlite3_stmt *	Sqlite3_stmt_val  (value) Pure;
static sqlite3_value *	Sqlite3_value_val (value) Pure;

static inline sqlite3 *
Sqlite3_val (value v)
{
  struct ml_sqlite3_data *data = Sqlite3_data_val (v);
  if (data->db == NULL)
    ml_sqlite3_raise_exn (SQLITE_MISUSE, "closed db", TRUE);
  return data->db;
}

static inline sqlite3_stmt *
Sqlite3_stmt_val (value v)
{
  sqlite3_stmt *stmt = * ((sqlite3_stmt **) v);
  if (stmt == NULL)
    ml_sqlite3_raise_exn (SQLITE_MISUSE, "invalid statement", TRUE);
  return stmt;
}

static inline sqlite3_value *
Sqlite3_value_val (value v)
{
  sqlite3_value *val = * ((sqlite3_value **) v);
  if (val == NULL)
    ml_sqlite3_raise_exn (SQLITE_MISUSE, "invalid value", TRUE);
  return val;
}
