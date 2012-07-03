module Weak_store = struct
  type 'a t = 
      { mutable w : 'a Weak.t ; 
        mutable free : int ; 
        finalise : 'a -> unit }

  let create f =
    { w = Weak.create 8 ; free = 0 ; finalise = f }

  let register s v =
    Gc.finalise s.finalise v ;
    let len = Weak.length s.w in
    assert (len > 0) ;
    if s.free < len
    then begin
      Weak.set s.w s.free (Some v) ;
      s.free <- s.free + 1
    end
    else begin
      let i = ref 0 in
      let full = ref true in
      while !full && !i < Weak.length s.w do
	full := Weak.check s.w !i ; 
	if !full then incr i
      done ;
      if !full
      then begin
	let n_s = Weak.create (2 * len) in
	Weak.blit s.w 0 n_s 0 len ;
	s.w <- n_s ; 
	s.free <- len + 1 ;
	i := len 
      end ;
      Weak.set s.w !i (Some v)
    end

  let clear s = 
      for i = 0 to Weak.length s.w - 1 do
	match Weak.get s.w i with
	| Some v -> s.finalise v
	| None -> ()
      done
end



type db
type stmt
type argument
type sql_type = [`INTEGER|`FLOAT|`TEXT|`BLOB|`NULL]
type sql_value = [
  | `INT of int
  | `INT64 of int64
  | `FLOAT of float
  | `TEXT of string
  | `BLOB of string
  | `VALUE of argument
  | `NULL ]

type error_code =
  | ERROR
  | INTERNAL
  | PERM
  | ABORT
  | BUSY
  | LOCKED
  | NOMEM
  | READONLY
  | INTERRUPT
  | IOERR
  | CORRUPT
  | NOTFOUND
  | FULL
  | CANTOPEN
  | PROTOCOL
  | EMPTY
  | SCHEMA
  | TOOBIG
  | CONSTRAINT
  | MISMATCH
  | MISUSE
  | NOLFS
  | AUTH
  | FORMAT
  | RANGE
  | NOTADB
exception Error of error_code * string

let init =
  Callback.register_exception "mlsqlite3_exn" (Error (ERROR, ""))


external open_db  : string -> db = "ml_sqlite3_open"
external _close_db : db -> unit = "ml_sqlite3_close"

external set_stmt_store : db -> stmt Weak_store.t option -> unit = "ml_sqlite3_set_stmt_store"
external get_stmt_store : db -> stmt Weak_store.t = "ml_sqlite3_get_stmt_store"

external finalize_stmt : stmt -> unit = "ml_sqlite3_finalize_noerr"

let stmt_store db =
  try get_stmt_store db
  with Not_found ->
    let s = Weak_store.create finalize_stmt in 
    set_stmt_store db (Some s) ;
    s
let register_stmt db stmt =
  Weak_store.register (stmt_store db) stmt

let close_db db =
  begin
    try Weak_store.clear (get_stmt_store db)
    with Not_found -> ()
  end ;
  _close_db db


external _compileoption_get : int -> string option = "ml_sqlite3_compileoption_get"
let compileoption_get () =
  let rec get i =
    match _compileoption_get i with
    | Some o -> o :: get (i+1)
    | None -> [] in
  get 0

external interrupt : db -> unit = "ml_sqlite3_interrupt"
external is_complete : string -> bool = "ml_sqlite3_complete"
external _version  : unit -> string = "ml_sqlite3_version"
let version = _version ()
external last_insert_rowid : db -> int64 = "ml_sqlite3_last_insert_rowid"
external changes       : db -> int = "ml_sqlite3_changes"
external total_changes : db -> int = "ml_sqlite3_total_changes"
external get_autocommit : db -> bool = "ml_sqlite3_get_autocommit"
external sleep : int -> unit = "ml_sqlite3_sleep"

external busy_set : db -> (int -> [`FAIL|`RETRY]) -> unit
   = "ml_sqlite3_busy_handler"
external busy_unset : db -> unit = "ml_sqlite3_busy_handler_unset"
external busy_timeout : db -> int -> unit = "ml_sqlite3_busy_timeout"

external trace_set   : db -> (string -> unit) -> unit = "ml_sqlite3_trace"
external trace_unset : db -> unit = "ml_sqlite3_trace_unset"

external progress_handler_set : db -> int -> (unit -> unit) -> unit 
  = "ml_sqlite3_progress_handler"
external progress_handler_unset : db -> unit = "ml_sqlite3_progress_handler_unset"


external prepare : db -> string -> int -> stmt option * int = "ml_sqlite3_prepare"
external reset : stmt -> unit = "ml_sqlite3_reset"
external expired : stmt -> bool = "ml_sqlite3_expired"
external step : stmt -> [`DONE|`ROW] = "ml_sqlite3_step"

external bind : stmt -> int -> sql_value -> unit = "ml_sqlite3_bind"
external bind_parameter_count : stmt -> int = "ml_sqlite3_bind_parameter_count"
external bind_parameter_index : stmt -> string -> int = "ml_sqlite3_bind_parameter_index"
external bind_parameter_name : stmt -> int -> string = "ml_sqlite3_bind_parameter_name"
external clear_bindings : stmt -> unit = "ml_sqlite3_clear_bindings"

external column_blob : stmt -> int -> string = "ml_sqlite3_column_blob"
external column_double : stmt -> int -> float = "ml_sqlite3_column_double"
external column_int : stmt -> int -> int = "ml_sqlite3_column_int"
external column_int64 : stmt -> int -> int64 = "ml_sqlite3_column_int64"
external column_text : stmt -> int -> string = "ml_sqlite3_column_text"
external column_type : stmt -> int -> sql_type = "ml_sqlite3_column_type"
external data_count : stmt -> int = "ml_sqlite3_data_count"
external column_count : stmt -> int = "ml_sqlite3_column_count"
external column_name : stmt -> int -> string = "ml_sqlite3_column_name"
external column_decltype : stmt -> int -> string = "ml_sqlite3_column_decltype"


external value_blob   : argument -> string = "ml_sqlite3_value_blob"
external value_double : argument -> float  = "ml_sqlite3_value_double"
external value_int    : argument -> int    = "ml_sqlite3_value_int"
external value_int64  : argument -> int64  = "ml_sqlite3_value_int64"
external value_text   : argument -> string = "ml_sqlite3_value_text"
external value_type   : argument -> sql_type = "ml_sqlite3_value_type"
external _create_function : 
  db -> string -> int -> (argument array -> sql_value) -> unit
    = "ml_sqlite3_create_function"

let create_fun_N db name f =
  _create_function db name (-1) f

let create_fun_0 db name f =
  _create_function db name 0 (fun _ -> f ())

let create_fun_1 db name f =
  _create_function db name 1 (fun args -> f args.(0))

let create_fun_2 db name f =
  _create_function db name 2 (fun args -> f args.(0) args.(1))

let create_fun_3 db name f =
  _create_function db name 3 (fun args -> f args.(0) args.(1) args.(2))

external delete_function : db -> string -> unit = "ml_sqlite3_delete_function"



(* Higher-level functions manipulating statements *)

type ('a, 'b) fmt = ('b, unit, string, 'a) format4 -> 'b

(* Prepare only the first statement of the SQL string *)
let rec _prepare_one db off sql =
  if off >= String.length sql
  then failwith "Sqlite3.prepare_one: empty statement" ;
  match prepare db sql off with
  | Some stmt, _ -> 
      register_stmt db stmt ;
      stmt
  | None, nxt -> 
      _prepare_one db nxt sql

let prepare_one db sql =
  _prepare_one db 0 sql

let prepare_one_f db fmt =
  Printf.kprintf (_prepare_one db 0) fmt



(* Loop over all the statements in a SQL string *)
let _fold_prepare ?(final=false) db sql f init =
  let rec loop acc off =
    if off >= String.length sql
    then acc
    else
      match prepare db sql off with
      | Some stmt, nxt -> 
	  register_stmt db stmt ;
	  let acc =
	    try f acc stmt
	    with exn when final -> 
	      finalize_stmt stmt ;
	      raise exn in
	  if final then finalize_stmt stmt ;
	  loop acc nxt
      | None, nxt -> 
	  loop acc nxt in
  loop init 0

let fold_prepare db sql f init =
  _fold_prepare db sql f init

let fold_prepare_f db fmt =
  Printf.kprintf (_fold_prepare db) fmt


(* Bind SQL values to statements *)
let do_bind stmt = function
  | [] -> []
  | l ->
      let n = bind_parameter_count stmt in
      let rec proc i = function
	| v :: tl when i <= n ->
	    bind stmt i v ;
	    proc (i+1) tl
	| l -> l in
      proc 1 l

let _fold_prepare_bind ?final db sql bindings f init =
  let bindings = ref bindings in
  _fold_prepare 
    ?final db sql
    (fun acc stmt ->
      bindings := do_bind stmt !bindings ;
      f acc stmt)
    init

let fold_prepare_bind db sql bindings f init =
  _fold_prepare_bind db sql bindings f init

let fold_prepare_bind_f db fmt =
  Printf.kprintf (_fold_prepare_bind db) fmt


(* Execute statements *)
let rec do_step stmt =
  match step stmt with
  | `DONE -> ()
  | `ROW  -> do_step stmt

let exec db sql =
  _fold_prepare
    ~final:true
    db sql
    (fun () stmt -> do_step stmt)
    ()

let exec_f db fmt =
  Printf.kprintf (exec db) fmt

let exec_v db sql data =
  _fold_prepare_bind
    ~final:true
    db sql data 
    (fun () stmt -> do_step stmt)
    ()

let exec_fv db fmt =
  Printf.kprintf (exec_v db) fmt



(* Execute statements and get some results back *)
let rec fold_step f acc stmt =
  match step stmt with
  | `DONE -> acc
  | `ROW  ->
      let acc =
	try f acc stmt
	with exn -> 
	  reset stmt ; 
	  raise exn in
      fold_step f acc stmt

let fetch db sql f init =
  _fold_prepare
    db sql
    (fold_step f) init

let fetch_f db fmt = 
  Printf.kprintf (fetch db) fmt

let fetch_v db sql data f init =
  _fold_prepare_bind 
    db sql data
    (fold_step f) init

let fetch_fv db fmt = 
  Printf.kprintf (fetch_v db) fmt



(* Reset-Bind-Step *)
let bind_and_exec stmt bindings =
  reset stmt ;
  ignore (do_bind stmt bindings) ;
  do_step stmt

let bind_fetch stmt bindings f init =
  reset stmt ;
  ignore (do_bind stmt bindings) ;
  fold_step f init stmt

let sql_escape s =
  let n = ref 0 in
  let len = String.length s in
  for i = 0 to len - 1 do
    let c = String.unsafe_get s i in
    if c = '\'' then incr n
  done ;
  if !n = 0
  then s
  else begin
    let n_len = len + !n in
    let o = String.create n_len in
    let j = ref 0 in
    for i = 0 to len - 1 do
      let c = String.unsafe_get s i in
      if c = '\'' then begin
	String.unsafe_set o !j '\'' ;
	incr j
      end ;
      String.unsafe_set o !j c ;
      incr j
    done ;
    assert (!j = n_len) ;
    o
  end

let char_of_hex v = 
  if v < 0xa
  then Char.chr (v + Char.code '0')
  else Char.chr (v - 0xa + Char.code 'a')

let hex_enc s =
  let len = String.length s in
  let o = String.create (2 * len) in
  for i = 0 to len - 1 do
    let c = int_of_char s.[i] in
    let hi = c lsr 4 in
    o.[2*i] <- char_of_hex hi ;
    let lo = c land 0xf in
    o.[2*i + 1] <- char_of_hex lo
  done ;
  o

let blob_escape = hex_enc

let string_of_transaction = function
  | `DEFERRED  -> "DEFERRED"
  | `IMMEDIATE -> "IMMEDIATE"
  | `EXCLUSIVE -> "EXCLUSIVE"

let transaction ?kind db f =
  begin
    let sql = 
      match kind with
      | None   -> "BEGIN"
      | Some t -> "BEGIN " ^ string_of_transaction t in
    exec db sql
  end ;
  try let r = f db in exec db "COMMIT" ; r
  with exn -> 
    begin try exec db "ROLLBACK" with _ -> () end ; 
    raise exn
