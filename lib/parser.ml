let stream_fold f a s =
  let state = ref a in
  Stream.iter (fun x -> state := f !state x) s;
  !state

let parse_fn l = function
  | '+' -> Ir.CellInc 1 :: l
  | '-' -> Ir.CellDec 1 :: l
  | '>' -> Ir.TapeInc 1 :: l
  | '<' -> Ir.TapeDec 1 :: l
  | ',' -> Ir.Read :: l
  | '.' -> Ir.Write :: l
  | '[' -> Ir.JumpIfZero (-1) :: l
  | ']' -> Ir.JumpIfNotZero (-1) :: l
  | _ -> l

let parse s =
  let stream = Stream.of_string s in
  (* use List.rev_map *)
  stream_fold parse_fn [] stream |> List.rev
