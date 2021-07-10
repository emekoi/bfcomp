type 't parsed = More of 't | Skip | End

let rec parse_char s = function
  | '+' -> More (Ir.Cell 1)
  | '-' -> More (Ir.Cell (-1))
  | '>' -> More (Ir.Tape 1)
  | '<' -> More (Ir.Tape (-1))
  | ',' -> More Ir.Read
  | '.' -> More Ir.Write
  (* is this reversing step expensive over time? *)
  | '[' -> More (Ir.Loop (parse_stream s |> List.rev))
  | ']' -> End
  | _ -> Skip

and parse_stream s =
  let acc = ref [] in
  let rec do_rec () =
    match Stream.peek s with
    | Some c -> (
        Stream.junk s;
        match parse_char s c with
        | More acc' ->
            acc := acc' :: !acc;
            do_rec ()
        | Skip -> do_rec ()
        | End -> ())
    | None -> ()
  in
  do_rec ();
  !acc

let parse s = Stream.of_string s |> parse_stream |> List.rev
