(** Implement the [REGEXP] SQL operator using OCaml [Str] module *)

val register   : Sqlite3.db -> unit
val unregister : Sqlite3.db -> unit
