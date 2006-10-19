#define CAML_NAME_SPACE

#include <caml/alloc.h>
#include <caml/memory.h>
#include <caml/fail.h>
#include <caml/custom.h>
#include <caml/bigarray.h>

#include <string.h>

#include <sqlite3.h>

#include "ocaml-sqlite3.h"

/* A global list of bigarrays that are currently bound in sqlite statements */
static value big_root;

static void
ml_sqlite3_register_big (value v)
{
  CAMLparam1(v);
  CAMLlocal1(c);
  /* initialize */
  if (big_root == 0)
    {
      big_root = Val_emptylist;
      caml_register_global_root (&big_root);
    }

  /* prepend it to the list */
  c = caml_alloc_small (2, Tag_cons);
  Field (c, 0) = v;
  Field (c, 1) = big_root;
  big_root = c;
  CAMLreturn0;
}

static void
ml_sqlite3_release_big (void *data)
{
  value c, p;
  p = Val_emptylist;
  c = big_root;
  while (c != Val_emptylist) 
    {
      void *d = Data_bigarray_val (Field (c, 0));
      value tl = Field (c, 1);

      if (d == data)
	{
	  if (p == Val_emptylist) /* c is the head */
	    big_root = tl;
	  else
	    Store_field(p, 1, tl);
	  return;
	}
      p = c;
      c = tl;
    }
  /* should not reach this point */
}


CAMLprim value
ml_sqlite3_bind_blob_big (value s, value idx, value v)
{
  sqlite3_stmt *stmt = Sqlite3_stmt_val (s);
  int i = Int_val (idx);
  int status;
  struct caml_bigarray *ba;

  ba = Bigarray_val (v);
  status = sqlite3_bind_blob (stmt, i, 
			      ba->data, ba->dim[0],
			      ml_sqlite3_release_big);

  if (status != SQLITE_OK)
    ml_sqlite3_raise_exn (status, "sqlite3_bind failed", TRUE);

  ml_sqlite3_register_big (v);
  return Val_unit;
}

CAMLprim value
ml_sqlite3_column_blob_big (value s, value i)
{
  CAMLparam1(s);
  CAMLlocal1(r);
  long len;
  const void * data;
  len  = sqlite3_column_bytes (Sqlite3_stmt_val (s), Int_val (i));
  r = alloc_bigarray (BIGARRAY_UINT8 | BIGARRAY_C_LAYOUT, 1, NULL, &len);
  data = sqlite3_column_blob (Sqlite3_stmt_val (s), Int_val(i));
  memcpy (Data_bigarray_val(r), data, len);
  CAMLreturn(r);
}
