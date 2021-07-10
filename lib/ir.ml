type t =
  | (* > | < *) Tape of int
  | (* + | - *) Cell of int
  | (* , *) Read
  | (* . *) Write
  | Set of int
  | Loop of t list
[@@deriving show]

module Pass = struct
  let finalize l = List.rev l

  let zero_loop code = function
    | Loop [ Cell _ ] -> Set 0 :: code
    | instr -> instr :: code

  let dead_code code instr =
    match code with
    | Loop _ :: _ -> ( match instr with Loop _ -> code | _ -> instr :: code)
    | _ -> instr :: code

  let contraction code instr =
    match code with
    | [] -> [ instr ]
    | head :: tail as l -> (
        match (instr, head) with
        | Tape a, Tape b -> Tape (a + b) :: tail
        | Cell a, Cell b -> Cell (a + b) :: tail
        | _ -> instr :: l)

  let run passes code =
    (* let f = function Loop body -> Loop (run passes body) | i -> i in *)
    let rec f pass code = function
      | Loop body -> pass code (Loop (g pass body |> List.rev))
      | i -> pass code i
    and g pass code = List.fold_left (f pass) [] code in

    let run_passes code pass = g pass code in
    let out = List.fold_left run_passes code passes in
    finalize out
end
