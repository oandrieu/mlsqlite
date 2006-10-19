open Sqlite3
open Bigarray

type t = (char, int8_unsigned_elt, c_layout) Array1.t

external bind : stmt -> int -> t -> unit = "ml_sqlite3_bind_blob_big"
external column_blob : stmt -> int -> t = "ml_sqlite3_column_blob_big"
