(** Sqlite3 bindings for OCaml *)

(** {2 Types and library initialization} *)

type db
type stmt
type argument

type sql_type = [ `BLOB | `FLOAT | `INTEGER | `NULL | `TEXT ]
type sql_value =
  [ `BLOB of string
  | `FLOAT of float
  | `INT of int
  | `INT64 of int64
  | `NULL
  | `TEXT of string
  | `VALUE of argument ]

type ('a, 'b) fmt = ('b, unit, string, 'a) format4 -> 'b

type error_code =
    ERROR      (** SQL error or missing database *)
  | INTERNAL   (** An internal logic error in SQLite *)
  | PERM       (** Access permission denied *)
  | ABORT      (** Callback routine requested an abort *)
  | BUSY       (** The database file is locked *)
  | LOCKED     (** A table in the database is locked *)
  | NOMEM      (** A [malloc()] failed *)
  | READONLY   (** Attempt to write a readonly database *)
  | INTERRUPT  (** Operation terminated by [sqlite_interrupt()] *)
  | IOERR      (** Some kind of disk I/O error occurred *)
  | CORRUPT    (** The database disk image is malformed *)
  | NOTFOUND   (** (Internal Only) Table or record not found *)
  | FULL       (** Insertion failed because database is full *)
  | CANTOPEN   (** Unable to open the database file *)
  | PROTOCOL   (** Database lock protocol error *)
  | EMPTY      (** (Internal Only) Database table is empty *)
  | SCHEMA     (** The database schema changed *)
  | TOOBIG     (** Too much data for one row of a table *)
  | CONSTRAINT (** Abort due to constraint violation *)
  | MISMATCH   (** Data type mismatch *)
  | MISUSE     (** Library used incorrectly *)
  | NOLFS      (** Uses OS features not supported on host *)
  | AUTH       (** Authorization denied *)
  | FORMAT     (** Auxiliary database format error *)
  | RANGE      (** 2nd parameter to [sqlite3_bind] out of range *)
  | NOTADB     (** File opened that is not a database file *)
exception Error of error_code * string

val version : string
(** The [sqlite3] library version number. *)

val init : unit
(** Reference this value to ensure that the [Sqlite3] module is linked in. *)


(** {2 Open/Close databases} *)

external open_db : string -> db = "ml_sqlite3_open"
val close_db : db -> unit

val compileoption_get : unit -> string list

external interrupt : db -> unit = "ml_sqlite3_interrupt"
external is_complete : string -> bool = "ml_sqlite3_complete"
external last_insert_rowid : db -> int64 = "ml_sqlite3_last_insert_rowid"
external changes : db -> int = "ml_sqlite3_changes"
external total_changes : db -> int = "ml_sqlite3_total_changes"
external get_autocommit : db -> bool = "ml_sqlite3_get_autocommit"
external sleep : int -> unit = "ml_sqlite3_sleep"

(** {2 Callbacks} *)

(** The [busy] callback *)

external busy_set     : db -> (int -> [ `FAIL | `RETRY ]) -> unit = "ml_sqlite3_busy_handler"
external busy_unset   : db -> unit = "ml_sqlite3_busy_handler_unset"
external busy_timeout : db -> int -> unit = "ml_sqlite3_busy_timeout"

(** The [trace] callback *)

external trace_set   : db -> (string -> unit) -> unit = "ml_sqlite3_trace"
external trace_unset : db -> unit = "ml_sqlite3_trace_unset"

(** The [progress] callback *)

external progress_handler_set   : db -> int -> (unit -> unit) -> unit = "ml_sqlite3_progress_handler"
external progress_handler_unset : db -> unit = "ml_sqlite3_progress_handler_unset"

(** {2 Compiled SQL statements } *)

external finalize_stmt : stmt -> unit = "ml_sqlite3_finalize_noerr"
(** Finalize a statement. Contrary to the C API of sqlite, it's {i not} necessary to
    finalize all statements before closing a [db]; the binding takes care of finalizing
    live statements referring to the [db] being closed. Of course, using a [stmt] after
    it's been manually finalized or after its [db] has been closed will raise a 
    {!Sqlite3.Error} exception (with the [MISUSE] error code).

    Statements are collected by the GC so it's not necessary to manually finalize them. *)

val prepare_one   : db -> string -> stmt
(** Prepare the first statement in a string. The rest of the string is ignored. *)
val prepare_one_f : db -> (stmt, 'a) fmt
(** Same as [prepare_one] but uses a format string.*)

external reset : stmt -> unit = "ml_sqlite3_reset"

external expired : stmt -> bool = "ml_sqlite3_expired"
(** This function is actually not a binding to the [sqlite3_expired] function.
    [expired] return [true] if the [stmt] was finalized by calling {!Sqlite3.finalize_stmt}
    or if its [db] was closed with {!Sqlite3.close_db} *)

external step : stmt -> [ `DONE | `ROW ] = "ml_sqlite3_step"
(** Call [sqlite3_step] on the statement. In case of error, a
    {!Sqlite3.Error} exception is raised and the [stmt] is reset, except if the error 
    is [BUSY] or [MISUSE]. *)

(** {3 SQL parameter binding} *)

external bind : stmt -> int -> sql_value -> unit = "ml_sqlite3_bind"
external bind_parameter_count : stmt -> int = "ml_sqlite3_bind_parameter_count"
external bind_parameter_index : stmt -> string -> int = "ml_sqlite3_bind_parameter_index"
external bind_parameter_name  : stmt -> int -> string = "ml_sqlite3_bind_parameter_name"
external clear_bindings    : stmt -> unit = "ml_sqlite3_clear_bindings"
external transfer_bindings : stmt -> stmt -> unit = "ml_sqlite3_transfer_bindings"

(** {3 Results} *)

external column_blob   : stmt -> int -> string = "ml_sqlite3_column_blob"
external column_double : stmt -> int -> float = "ml_sqlite3_column_double"
external column_int    : stmt -> int -> int = "ml_sqlite3_column_int"
external column_int64  : stmt -> int -> int64 = "ml_sqlite3_column_int64"
external column_text   : stmt -> int -> string = "ml_sqlite3_column_text"

external column_type   : stmt -> int -> sql_type = "ml_sqlite3_column_type"
external data_count    : stmt -> int = "ml_sqlite3_data_count"
external column_count  : stmt -> int = "ml_sqlite3_column_count"
external column_name   : stmt -> int -> string = "ml_sqlite3_column_name"
external column_decltype : stmt -> int -> string = "ml_sqlite3_column_decltype"

(** {2 User-defined SQL functions } *)

(** {3 Arguments access} *)

external value_blob   : argument -> string = "ml_sqlite3_value_blob"
external value_double : argument -> float = "ml_sqlite3_value_double"
external value_int    : argument -> int = "ml_sqlite3_value_int"
external value_int64  : argument -> int64 = "ml_sqlite3_value_int64"
external value_text   : argument -> string = "ml_sqlite3_value_text"
external value_type   : argument -> sql_type = "ml_sqlite3_value_type"

(** {3 Registration} *)

val create_fun_N : db -> string -> (argument array -> sql_value) -> unit
val create_fun_0 : db -> string -> (unit -> sql_value) -> unit
val create_fun_1 : db -> string -> (argument -> sql_value) -> unit
val create_fun_2 : db -> string -> (argument -> argument -> sql_value) -> unit
val create_fun_3 : db -> string -> (argument -> argument -> argument -> sql_value) -> unit

external delete_function : db -> string -> unit = "ml_sqlite3_delete_function"


(** {2 High-level functions} *)

val do_step   : stmt -> unit
(** Repeatedly apply {!Sqlite3.step} to the statement until [`DONE] is returned *)
val fold_step : ('a -> stmt -> 'a) -> 'a -> stmt -> 'a
(** [fold_step f init stmt] repeatedly applies {!Sqlite3.step} to
    [stmt] and calls [f] on each row. The [stmt] is reset if the
    evaluation of [f] raises an exception. *)

val bind_and_exec : stmt -> sql_value list -> unit
(** Reset the [stmt], bind values, then call {!Sqlite3.do_step} *)
val bind_fetch    : stmt -> sql_value list -> ('a -> stmt -> 'a) -> 'a -> 'a
(** Reset the [stmt], bind values, then call {!Sqlite3.fold_step} *)

val fold_prepare        : db -> string -> ('a -> stmt -> 'a) -> 'a -> 'a
(** [fold_prepare db sql f init] prepares all statements in the string [sql],
    calling [f] for each one. *)
val fold_prepare_bind   : db -> string -> sql_value list -> ('a -> stmt -> 'a) -> 'a -> 'a
(** [fold_prepare_bind db sql values f init] prepares all statements in the string [sql],
    binding values from the list [values] and then calling [f] for each one. 

    The number of parameters to bind for each statement is determined by
    {!Sqlite3.bind_parameter_count}.
 *)

val exec     : db -> string -> unit
(** For each statement in the string, prepare it and execute it.

   Combines {!Sqlite3.fold_prepare} and {!Sqlite3.do_step} *)
val fetch    : db -> string -> ('a -> stmt -> 'a) -> 'a -> 'a
(** For each statement in the string, prepare it, execute it and apply the third argument 
   for each returned row. 

   Combines {!Sqlite3.fold_prepare} and {!Sqlite3.fold_step} *)

val exec_v   : db -> string -> sql_value list -> unit
(** Combines {!Sqlite3.fold_prepare_bind} and {!Sqlite3.do_step} *)
val fetch_v  : db -> string -> sql_value list -> ('a -> stmt -> 'a) -> 'a -> 'a
(** Combines {!Sqlite3.fold_prepare_bind} and {!Sqlite3.fold_step} *)

(** The following functions are variants that use a format string instead of 
    a plain string for the SQL program. *) 

val fold_prepare_f      : db -> (('a -> stmt -> 'a) -> 'a -> 'a, 'b) fmt
val fold_prepare_bind_f : db -> (sql_value list -> ('a -> stmt -> 'a) -> 'a -> 'a, 'b) fmt

val exec_f   : db -> (unit, 'b) fmt
val fetch_f  : db -> (('a -> stmt -> 'a) -> 'a -> 'a, 'b) fmt

val exec_fv  : db -> (sql_value list -> unit, 'b) fmt
val fetch_fv : db -> (sql_value list -> ('a -> stmt -> 'a) -> 'a -> 'a, 'b) fmt

(** {2 Convenience functions} *)

val sql_escape  : string -> string
(** Escape a string according to SQL rules: any quote character ['] is doubled.

    [sql_escape "abc'def"] thus returns ["abc''def"]. *)

val blob_escape : string -> string
(** Base-16 encode a string, suitably for BLOB literals ([X'abcd'] notation).

    [blob_escape "A\n"] thus returns ["410a"]. *)

val transaction : 
  ?kind:[`DEFERRED|`IMMEDIATE|`EXCLUSIVE] ->
  db -> (db -> 'a) -> 'a
(** Evaluate a function within a transaction. [BEGIN] is executed, then the function 
    argument is applied to the [db], then [COMMIT] is executed and the result of the
    function is returned. If the function raises an exception, [ROLLACK] is executed. *)
