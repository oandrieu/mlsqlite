#include <string.h>
#include <assert.h>

#define CAML_NAME_SPACE

#include <caml/alloc.h>
#include <caml/memory.h>
#include <caml/fail.h>
#include <caml/callback.h>
#include <caml/custom.h>

#include <sqlite3.h>

#include "ocaml-sqlite3.h"

/* Not wrapped :
   - user-defined aggregate functions
   - collation functions
   - sqlite3_db_handle -> should not be wrapped !
   - sqlite3_commit_hook
   - sqlite3_global_recover
   - sqlite3_set_authorizer
*/

/* compatibility for OCaml < 3.10 */
#ifndef CAMLreturnT
# define CAMLreturnT(t,e) CAMLreturn(e)
#endif



/* Error handling */

void ml_sqlite3_raise_exn (int status, const char *errmsg, int static_errmsg)
{
  static value *sqlite3_exn;

  /* check this is really an error */
  assert (status > SQLITE_OK && status < SQLITE_ROW);

  if (sqlite3_exn == NULL)
    {
      sqlite3_exn = caml_named_value ("mlsqlite3_exn");
      if (sqlite3_exn == NULL)
	caml_failwith ("Sqlite3 exception not registered");
    }

  { 
    CAMLlocal1(bucket);
    bucket = caml_alloc (3, 0);
    Store_field(bucket, 0, *sqlite3_exn);
    Store_field(bucket, 1, Val_long (status - 1));
    Store_field(bucket, 2, caml_copy_string (errmsg ? (char *) errmsg : ""));
    if (! static_errmsg)
      sqlite3_free ((char *) errmsg);
    caml_raise (bucket);
  }
}


static value *
ml_sqlite3_global_root_new (value v)
{
  value *p = caml_stat_alloc (sizeof (value));
  *p = v;
  caml_register_global_root (p);
  return p;
}

static void
ml_sqlite3_global_root_destroy (void *data)
{
  value *p = data;
  caml_remove_global_root (p);
  caml_stat_free (data);
}



/* 0 -> busy
 * 1 -> trace
 * 2 -> progress
 */
#define NUM_CALLBACKS 3

static void 
ml_finalize_sqlite3 (value v)
{
  struct ml_sqlite3_data *data = Sqlite3_data_val(v);
  caml_remove_global_root (&data->callbacks);
  caml_remove_global_root (&data->stmt_store);
  caml_stat_free (data);
}

static value
ml_wrap_sqlite3 (sqlite3 *db)
{
  static struct custom_operations ops = {
    "mlsqlite3/001", 
    ml_finalize_sqlite3,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
  };
  CAMLparam0();
  CAMLlocal1(v);
  struct ml_sqlite3_data **store, *data;
  data = caml_stat_alloc (sizeof *data);
  v = caml_alloc_custom (&ops, sizeof data, 1, 100);
  store = Data_custom_val (v);
  *store = data;
  data->db = db;
  data->callbacks = caml_alloc (NUM_CALLBACKS, 0);
  data->stmt_store = Val_unit;
  caml_register_global_root (&data->callbacks);
  caml_register_global_root (&data->stmt_store);
  CAMLreturn(v);
}

CAMLprim value
ml_sqlite3_open (value filename)
{
  sqlite3 *db;
  int status;

  status = sqlite3_open (String_val(filename), &db);
  if (status != SQLITE_OK)
    {
      char *errmsg;
      errmsg = (char *) sqlite3_errmsg (db);
      sqlite3_close (db); /* ignore status here */
      ml_sqlite3_raise_exn (status, errmsg, TRUE);
    }

  return ml_wrap_sqlite3 (db);
}

CAMLprim value
ml_sqlite3_close (value db)
{
  struct ml_sqlite3_data *data = Sqlite3_data_val(db);
  if (data->db != NULL)
    {
      int status;
      status = sqlite3_close (data->db);
      if (status != SQLITE_OK)
	raise_sqlite3_exn (db);
      data->db = NULL;
    }
  return Val_unit;
}

CAMLprim value
ml_sqlite3_set_stmt_store (value db, value s)
{
  struct ml_sqlite3_data *data = Sqlite3_data_val (db);
  if (Is_block (s))
    data->stmt_store = Field (s, 0);
  else
    data->stmt_store = Val_unit;
  return Val_unit;
}

CAMLprim value
ml_sqlite3_get_stmt_store (value db, value s)
{
  struct ml_sqlite3_data *data = Sqlite3_data_val (db);
  if (data->stmt_store == Val_unit)
    caml_raise_not_found();
  return data->stmt_store;
}



/* Misc general functions */

CAMLprim value 
ml_sqlite3_interrupt (value db)
{
  sqlite3_interrupt (Sqlite3_val (db));
  return Val_unit;
}

CAMLprim value
ml_sqlite3_complete (value sql)
{
#ifdef HAVE_SQLITE3_COMPLETE
  return Val_bool (sqlite3_complete (String_val (sql)));
#else
  caml_failwith ("sqlite3_complete unavailable");
#endif
}

CAMLprim value 
ml_sqlite3_version (value unit)
{
  return caml_copy_string (sqlite3_version);
}

CAMLprim value
ml_sqlite3_last_insert_rowid (value db)
{
  return caml_copy_int64 (sqlite3_last_insert_rowid (Sqlite3_val (db)));
}

CAMLprim value
ml_sqlite3_changes (value db)
{
  return Val_long (sqlite3_changes (Sqlite3_val (db)));
}

CAMLprim value
ml_sqlite3_total_changes (value db)
{
  return Val_long (sqlite3_total_changes (Sqlite3_val (db)));
}

CAMLprim value
ml_sqlite3_get_autocommit (value db)
{
#ifdef HAVE_SQLITE3_GET_AUTOCOMMIT
  return Val_bool (sqlite3_get_autocommit (Sqlite3_val (db)));
#else
  caml_failwith ("sqlite3_get_autocommit unavailable");
#endif
}

CAMLprim value
ml_sqlite3_sleep (value ms)
{
#if HAVE_SQLITE3_SLEEP
  return Val_int (sqlite3_sleep (Int_val (ms)));
#else
  caml_failwith ("sqlite3_sleep unavailable");
#endif
}

CAMLprim value
ml_sqlite3_compileoption_get(value i)
{
  CAMLparam0();
  CAMLlocal2(v, s);
  const char *o;
  o = sqlite3_compileoption_get(Int_val(i));
  if (o) {
    s = caml_copy_string(o);
    v = caml_alloc_small(1, 0);
    Field(v, 0) = s;
  }
  else {
    v = Val_long(0);
  }
  CAMLreturn(v);
}



/* callbacks */

#define MLTAG_RETRY -915497327L
#define MLTAG_FAIL  1559036861L

static int
ml_sqlite3_busy_handler_cb (void *data, int num)
{
  struct ml_sqlite3_data *db = data;
  value res;
  res = caml_callback_exn (Field (db->callbacks, 0), Val_int (num));
  if (Is_exception_result (res))
    return 0;
  return (res == MLTAG_RETRY);
}

CAMLprim value
ml_sqlite3_busy_handler (value db, value cb)
{
  struct ml_sqlite3_data *db_data = Sqlite3_data_val(db);
  sqlite3_busy_handler (Sqlite3_val(db), 
			ml_sqlite3_busy_handler_cb, db_data);
  Store_field (db_data->callbacks, 0, cb);
  return Val_unit;
}

CAMLprim value 
ml_sqlite3_busy_handler_unset (value db)
{
  struct ml_sqlite3_data *db_data = Sqlite3_data_val(db);
  sqlite3_busy_handler (Sqlite3_val(db), NULL, NULL);
  Store_field (db_data->callbacks, 0, Val_unit);
  return Val_unit;
}

CAMLprim value 
ml_sqlite3_busy_timeout (value db, value t)
{
  sqlite3_busy_timeout (Sqlite3_val (db), Int_val (t));
  return Val_unit;
}

static void 
ml_sqlite3_trace_handler (void *data, const char *req)
{
 struct ml_sqlite3_data *db = data;
 value s = caml_copy_string (req);
 caml_callback_exn (Field (db->callbacks, 1), s);
}

CAMLprim value 
ml_sqlite3_trace (value db, value cb)
{
  struct ml_sqlite3_data *db_data = Sqlite3_data_val(db);
  sqlite3_trace (Sqlite3_val (db), ml_sqlite3_trace_handler, db_data);
  Store_field (db_data->callbacks, 1, cb);
  return Val_unit;
}

CAMLprim value 
ml_sqlite3_trace_unset (value db)
{
  struct ml_sqlite3_data *db_data = Sqlite3_data_val(db);
  sqlite3_trace (Sqlite3_val (db), NULL, NULL);
  Store_field (db_data->callbacks, 1, Val_unit);
  return Val_unit;
}

#ifdef HAVE_SQLITE3_PROGRESS_HANDLER
static int 
ml_sqlite3_progress_handler_cb (void *data)
{
 struct ml_sqlite3_data *db = data;
 value res;
 res = caml_callback_exn (Field (db->callbacks, 2), Val_unit);
 return Is_exception_result(res);
}
#endif

CAMLprim value 
ml_sqlite3_progress_handler (value db, value delay, value cb)
{
#ifdef HAVE_SQLITE3_PROGRESS_HANDLER
  struct ml_sqlite3_data *db_data = Sqlite3_data_val(db);
  sqlite3_progress_handler (Sqlite3_val (db), Int_val (delay), 
			    ml_sqlite3_progress_handler_cb, db_data);
  Store_field (db_data->callbacks, 2, cb);
#endif
  return Val_unit;
}

CAMLprim value 
ml_sqlite3_progress_handler_unset (value db)
{
#ifdef HAVE_SQLITE3_PROGRESS_HANDLER
  struct ml_sqlite3_data *db_data = Sqlite3_data_val(db);
  sqlite3_progress_handler (Sqlite3_val(db), 0, NULL, NULL);
  Store_field (db_data->callbacks, 2, Val_unit);
#endif
  return Val_unit;
}



#define MLTAG_INTEGER  769598269L
#define MLTAG_FLOAT     17431289L
#define MLTAG_TEXT    1869949275L
#define MLTAG_BLOB    1471417019L
#define MLTAG_NULL    1738460431L

#define MLTAG_INT        7295391L
#define MLTAG_INT64   2015220635L
#define MLTAG_VALUE   1598910115L

static value
convert_sqlite3_type (int t)
{
  switch (t)
    {
    case SQLITE_INTEGER:
      return MLTAG_INTEGER;
    case SQLITE_FLOAT:
      return MLTAG_FLOAT;
    case SQLITE_TEXT:
      return MLTAG_TEXT;
    case SQLITE_BLOB:
      return MLTAG_BLOB;
    default:
      return MLTAG_NULL;
    }
}




/* Prepared statements */

CAMLprim value
ml_sqlite3_finalize_noerr (value s)
{
  sqlite3_stmt **p_stmt = (sqlite3_stmt **) Field (s, 0);

  if (*p_stmt != NULL)
    {
      sqlite3_finalize (*p_stmt);
      *p_stmt = NULL;
    }
  return Val_unit;
}

static sqlite3_stmt *
ml_sqlite3_prepare_stmt (value db, value sql, value sql_off, unsigned int *tail_pos)
{
  CAMLparam2(db, sql);
  sqlite3_stmt *stmt = NULL;
  const char *tail;
  int status;
  unsigned int off = Unsigned_int_val (sql_off);
  status = sqlite3_prepare (Sqlite3_val (db), 
			    String_val (sql) + off,
			    caml_string_length (sql) - off,
			    &stmt, &tail);
  if (status != SQLITE_OK)
    {
      if (stmt != NULL)
	sqlite3_finalize (stmt);
      raise_sqlite3_exn (db);
    }
  if (tail_pos != NULL)
    *tail_pos = tail - String_val (sql);
  CAMLreturnT (sqlite3_stmt *, stmt);
}

CAMLprim value
ml_sqlite3_prepare (value db, value sql, value sql_off)
{
  CAMLparam2(db, sql);
  CAMLlocal4(t, o, r, s);
  sqlite3_stmt *stmt;
  unsigned int tail_pos;

  stmt = ml_sqlite3_prepare_stmt (db, sql, sql_off, &tail_pos);
  if (stmt == NULL)
    o = Val_unit;
  else
    {
      s = caml_alloc_small (1, Abstract_tag);
      Field (s, 0) = Val_bp (stmt);
      r = caml_alloc_small (4, 0);
      Field (r, 0) = s;
      Field (r, 1) = db;
      Field (r, 2) = sql;
      Field (r, 3) = sql_off;
      o = caml_alloc_small (1, 0);
      Field (o, 0) = r;
    }
  t = caml_alloc_small (2, 0);
  Field (t, 0) = o;
  Field (t, 1) = Val_int (tail_pos);
  CAMLreturn (t);
}

static sqlite3_stmt *
ml_sqlite3_recompile (value v, sqlite3_stmt *old_stmt)
{
  CAMLparam1(v);
  CAMLlocal1(s);
  sqlite3_stmt *stmt;

  stmt = ml_sqlite3_prepare_stmt (Field (v, 1), Field (v, 2), Field (v, 3), NULL);
  if (stmt == NULL)
    caml_failwith ("Sqlite3.recompile");
  if (old_stmt != NULL)
    {
      sqlite3_transfer_bindings (old_stmt, stmt);
      sqlite3_finalize (old_stmt);
    }

  s = caml_alloc_small (1, Abstract_tag);
  Field (s, 0) = Val_bp (stmt);
  Store_field (v, 0, s);
  CAMLreturnT (sqlite3_stmt *, stmt);
}

CAMLprim value
ml_sqlite3_reset (value stmt)
{
  sqlite3_stmt *s = Sqlite3_stmt_val (stmt);
  sqlite3_reset (s);
  return Val_unit;
}

CAMLprim value
ml_sqlite3_expired (value stmt)
{
  sqlite3_stmt *s = * ((sqlite3_stmt **) Field (stmt, 0));
  return Val_bool (s == NULL);
}

#define MLTAG_ROW	   8190965L
#define MLTAG_DONE	1516073221L

CAMLprim value
ml_sqlite3_step (value stmt)
{
  CAMLparam1(stmt);
  CAMLlocal1(r);
  int status;
  sqlite3_stmt *s = Sqlite3_stmt_val (stmt);

 again:
  status = sqlite3_step (s);
  switch (status)
    {
    case SQLITE_ROW:
      r = MLTAG_ROW; break;
    case SQLITE_DONE:
      r = MLTAG_DONE; break;
    default: /* either BUSY, ERROR or MISUSE */
      {
	sqlite3 *db;
	if (status == SQLITE_ERROR)
	  status = sqlite3_reset (s);
	if (status == SQLITE_SCHEMA)
	  {
	    s = ml_sqlite3_recompile (stmt, s);
	    goto again;
	  }
	db = sqlite3_db_handle (s);
	ml_sqlite3_raise_exn (status, sqlite3_errmsg (db), TRUE);
      }
    }
  CAMLreturn (r);
}



/* sqlite3_bind_* */

CAMLprim value
ml_sqlite3_bind (value s, value idx, value v)
{
  sqlite3_stmt *stmt = Sqlite3_stmt_val (s);
  int i = Int_val (idx);
  int status;

  if (Is_long (v))
    status = sqlite3_bind_null (stmt, i);
  else
    {
      value val = Field (v, 1);
      switch (Field (v, 0))
	{
	case MLTAG_INT:
	  status = sqlite3_bind_int (stmt, i, Int_val (val)); break;
	case MLTAG_INT64:
	  status = sqlite3_bind_int64 (stmt, i, Int64_val (val)); break;
	case MLTAG_FLOAT:
	  status = sqlite3_bind_double (stmt, i, Double_val (val)); break;
	case MLTAG_TEXT:
	  status = sqlite3_bind_text (stmt, i, 
				      String_val (val), 
				      caml_string_length (val), 
				      SQLITE_TRANSIENT);
	  break;
	case MLTAG_BLOB:
	  status = sqlite3_bind_blob (stmt, i, 
				      String_val (val), 
				      caml_string_length (val), 
				      SQLITE_TRANSIENT);
	  break;
	case MLTAG_VALUE:
#if HAVE_SQLITE3_BIND_VALUE
	  status = sqlite3_bind_value (stmt, i, Sqlite3_value_val (val)); break;
#else
	  caml_failwith ("sqlite3_bind_value unavailable");
#endif
	default:
	  status = SQLITE_MISUSE;
	}
    }
  if (status != SQLITE_OK)
    ml_sqlite3_raise_exn (status, "sqlite3_bind failed", TRUE);
  return Val_unit;
}

CAMLprim value
ml_sqlite3_bind_parameter_count (value s)
{
  return Val_int (sqlite3_bind_parameter_count (Sqlite3_stmt_val (s)));
}

CAMLprim value
ml_sqlite3_bind_parameter_index (value s, value n)
{
  return Val_int (sqlite3_bind_parameter_index (Sqlite3_stmt_val (s),
						String_val(n)));
}

CAMLprim value
ml_sqlite3_bind_parameter_name (value s, value idx)
{
  return caml_copy_string (sqlite3_bind_parameter_name (Sqlite3_stmt_val (s),
							Int_val (idx)));
}

CAMLprim value
ml_sqlite3_clear_bindings (value s)
{
#if HAVE_SQLITE3_CLEAR_BINDINGS
  int status;
  status = sqlite3_clear_bindings (Sqlite3_stmt_val (s));
  if (status != SQLITE_OK)
    ml_sqlite3_raise_exn (status, "clear_bindings failed", TRUE);
  return Val_unit;
#else
  sqlite3_stmt *stmt = Sqlite3_stmt_val (s);
  int i, n, status;
  n = sqlite3_bind_parameter_count(stmt);
  for (i = 1; i <= n; i++) 
    {
      status = sqlite3_bind_null(stmt, i);
      if (status != SQLITE_OK)
	ml_sqlite3_raise_exn (status, "clear_bindings failed", TRUE);
    }
  return Val_unit;
#endif
}

CAMLprim value
ml_sqlite3_transfer_bindings (value s1, value s2)
{
  int status;
  status = sqlite3_transfer_bindings (Sqlite3_stmt_val (s1),
				      Sqlite3_stmt_val (s2));
  if (status != SQLITE_OK)
    ml_sqlite3_raise_exn (status, "transfer_bindings failed", TRUE);
  return Val_unit;
}


/* sqlite3_column_* */

CAMLprim value
ml_sqlite3_column_blob (value s, value i)
{
  CAMLparam1(s);
  CAMLlocal1(r);
  int len;
  const void * data;
  len = sqlite3_column_bytes (Sqlite3_stmt_val (s), Int_val (i));
  r = caml_alloc_string (len);
  data = sqlite3_column_blob (Sqlite3_stmt_val (s), Int_val(i));
  memcpy (Bp_val (r), data, len);
  CAMLreturn(r);
}

CAMLprim value
ml_sqlite3_column_double (value s, value i)
{
  return caml_copy_double (sqlite3_column_double (Sqlite3_stmt_val (s), Int_val(i)));
}

CAMLprim value
ml_sqlite3_column_int (value s, value i)
{
  return Val_int (sqlite3_column_int (Sqlite3_stmt_val (s), Int_val(i)));
}

CAMLprim value
ml_sqlite3_column_int64 (value s, value i)
{
  return caml_copy_int64 (sqlite3_column_int64 (Sqlite3_stmt_val (s), Int_val(i)));
}

CAMLprim value
ml_sqlite3_column_text (value s, value i)
{
  CAMLparam1(s);
  CAMLlocal1(r);
  int len;
  const void * data;
  len = sqlite3_column_bytes (Sqlite3_stmt_val (s), Int_val (i));
  r = caml_alloc_string (len);
  data = sqlite3_column_text (Sqlite3_stmt_val (s), Int_val(i));
  memcpy (Bp_val (r), data, len);
  CAMLreturn(r);
}

CAMLprim value
ml_sqlite3_column_type (value s, value i)
{
  return convert_sqlite3_type (sqlite3_column_type (Sqlite3_stmt_val (s), Int_val(i)));
}

CAMLprim value
ml_sqlite3_data_count (value s)
{
  return Val_int (sqlite3_data_count (Sqlite3_stmt_val (s)));
}

CAMLprim value
ml_sqlite3_column_count (value s)
{
  return Val_int (sqlite3_column_count (Sqlite3_stmt_val (s)));
}

CAMLprim value
ml_sqlite3_column_name (value s, value i)
{
  return caml_copy_string (sqlite3_column_name (Sqlite3_stmt_val (s), 
						Int_val(i)));
}

CAMLprim value
ml_sqlite3_column_decltype (value s, value i)
{
  return caml_copy_string (sqlite3_column_decltype (Sqlite3_stmt_val (s),
						    Int_val(i)));
}


/* User-defined functions */

static value
ml_sqlite3_wrap_values (int argc, sqlite3_value **args)
{
  int i;
  CAMLparam0();
  CAMLlocal2(a, v);
  if (argc <= 0 || args == NULL)
    CAMLreturn (Atom (0));
  a = caml_alloc (argc, 0);
  for (i=0; i<argc; i++)
    {
      v = caml_alloc_small (1, Abstract_tag);
      Field (v, 0) = Val_bp (args[i]);
      Store_field (a, i, v);
    }
  CAMLreturn (a);
}

static void
ml_sqlite3_wipe_values (value a)
{
  mlsize_t i, len = Wosize_val (a);
  for (i=0; i<len; i++)
    Store_field (Field (a, i), 0, 0);
}

CAMLprim value
ml_sqlite3_value_blob (value v)
{
  CAMLparam1(v);
  CAMLlocal1(r);
  int len;
  const void *data;
  len = sqlite3_value_bytes (Sqlite3_value_val (v));
  r = caml_alloc_string (len);
  data = sqlite3_value_blob (Sqlite3_value_val (v));
  memcpy (Bp_val (r), data, len);
  CAMLreturn(r);
}

CAMLprim value
ml_sqlite3_value_double (value v)
{
  return caml_copy_double (sqlite3_value_double (Sqlite3_value_val (v)));
}

CAMLprim value
ml_sqlite3_value_int (value v)
{
  return Val_long (sqlite3_value_int (Sqlite3_value_val (v)));
}

CAMLprim value
ml_sqlite3_value_int64 (value v)
{
  return caml_copy_int64 (sqlite3_value_int64 (Sqlite3_value_val (v)));
}

CAMLprim value
ml_sqlite3_value_text (value v)
{
  CAMLparam1(v);
  CAMLlocal1(r);
  int len;
  const void *data;
  len = sqlite3_value_bytes (Sqlite3_value_val (v));
  r = caml_alloc_string (len);
  data = sqlite3_value_text (Sqlite3_value_val (v));
  memcpy (Bp_val (r), data, len);
  CAMLreturn(r);
}

CAMLprim value
ml_sqlite3_value_type (value v)
{
  return convert_sqlite3_type (sqlite3_value_type (Sqlite3_value_val (v)));
}

static void 
ml_sqlite3_set_result (sqlite3_context *ctx, value res)
{
  if (Is_exception_result (res))
    sqlite3_result_error (ctx, "ocaml callback raised an exception", -1);
  else if (Is_long (res))
    sqlite3_result_null (ctx);
  else
    {
      value v = Field (res, 1);
      switch (Field (res, 0))
	{
	case MLTAG_INT:
	  sqlite3_result_int (ctx, Long_val (v));
	  break;
	case MLTAG_INT64:
	  sqlite3_result_int64 (ctx, Int64_val (v));
	  break;
	case MLTAG_FLOAT:
	  sqlite3_result_double (ctx, Double_val (v));
	  break;
	case MLTAG_TEXT:
	  sqlite3_result_text (ctx, String_val (v), caml_string_length (v), SQLITE_TRANSIENT);
	  break;
	case MLTAG_BLOB:
	  sqlite3_result_blob (ctx, String_val (v), caml_string_length (v), SQLITE_TRANSIENT);
	  break;
	case MLTAG_VALUE:
	  {
	    sqlite3_result_value (ctx, Sqlite3_value_val (v));
	    break;
	  }
	default:
	  sqlite3_result_error (ctx, "unknown value returned by callback", -1);
	}
    }
}

static void
ml_sqlite3_user_function (sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  value *fun = sqlite3_user_data (ctx);
  CAMLparam0();
  CAMLlocal2(res, args);

  args = ml_sqlite3_wrap_values (argc, argv);
  res = caml_callback_exn (*fun, args);
  ml_sqlite3_set_result (ctx, res);
  ml_sqlite3_wipe_values (args);
  CAMLreturn0;
}

CAMLprim value
ml_sqlite3_create_function (value db, value name, value nargs, value fun)
{
  CAMLparam3(db, name, fun);
  int status;
  sqlite3 *s_db = Sqlite3_val(db);
  value *param;

  param = ml_sqlite3_global_root_new(fun);
  status = sqlite3_create_function_v2 (s_db, String_val (name),
                                       Int_val (nargs), SQLITE_UTF8, param,
                                       ml_sqlite3_user_function, NULL, NULL,
                                       ml_sqlite3_global_root_destroy);
  if (status != SQLITE_OK)
    raise_sqlite3_exn (db);
  CAMLreturn(Val_unit);
}

CAMLprim value
ml_sqlite3_delete_function (value db, value name)
{
  int status;
  sqlite3 *s_db = Sqlite3_val(db);
  status = sqlite3_create_function_v2 (s_db, String_val (name), 
                                       0, SQLITE_UTF8, NULL, 
                                       NULL, NULL, NULL,
                                       NULL);
  if (status != SQLITE_OK)
    raise_sqlite3_exn (db);
  return Val_unit;
}
