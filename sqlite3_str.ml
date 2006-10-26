
let sql_value_of_bool b =
  if b then `INT 1 else `INT 0

let sqlite_regexp arg_p arg_t =
  let p = Sqlite3.value_text arg_p in
  let t = Sqlite3.value_text arg_t in
  sql_value_of_bool
    (Str.string_match 
       (Str.regexp p) t 0)

let register db =
  Sqlite3.create_fun_2 db "regexp" sqlite_regexp

let unregister db =
  Sqlite3.delete_function db "regexp"
