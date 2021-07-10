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

  let zero_loop input =
    let f code instr =
      match instr with Loop [ Cell _ ] -> Set 0 :: code | _ -> instr :: code
    in
    List.fold_left f [] input

  let dead_code input =
    let f code instr =
      match code with
      | Loop _ :: _ -> (
          match instr with
          | Loop _ ->
              let () = print_endline "deleting code" in
              code
          | loop -> loop :: code)
      | _ -> instr :: code
    in
    List.fold_left f [] input

  let contraction =
    let contract code instr =
      match code with
      | [] -> [ instr ]
      | head :: tail as l -> (
          match (instr, head) with
          | Tape a, Tape b -> Tape (a + b) :: tail
          | Cell a, Cell b -> Cell (a + b) :: tail
          | _ -> instr :: l)
    in

    let rec f code = function
      | Loop body -> Loop (g body |> List.rev) :: code
      | i -> contract code i
    and g code = List.fold_left f [] code in
    g

  let run passes code =
    (* let f = function Loop body -> Loop (run passes body) | i -> i in *)
    let f = Fun.id in
    let run_passes code pass = List.map f code |> pass in
    let out = List.fold_left run_passes code passes in
    finalize out
end
