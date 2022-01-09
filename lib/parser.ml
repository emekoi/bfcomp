type 't parsed = More of 't | Skip | End

let rec parse_char s c k =
  k (match c with
      | '+' -> More (Ir.Cell 1)
      | '-' -> More (Ir.Cell (-1))
      | '>' -> More (Ir.Tape 1)
      | '<' -> More (Ir.Tape (-1))
      | ',' -> More Ir.Read
      | '.' -> More Ir.Write
      (* is this reversing step expensive over time? *)
      | '[' -> More (Loop (parse_stream s |> List.rev))
      | ']' -> End
      | _ -> Skip)

and parse_stream s =
  let rec do_rec acc =
    match Stream.peek s with
    | Some c -> (
        Stream.junk s;
        parse_char s c (function
            | More acc' ->
              do_rec (acc' :: acc)
            | Skip -> do_rec acc
            | End -> acc))
    | None -> acc
  in
  do_rec []

let parse s = Stream.of_string s |> parse_stream |> List.rev
