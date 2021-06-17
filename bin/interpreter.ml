open Bfcomp.Ir

let imod a b =
  let m = a mod b in
  if m >= 0 then m else m + b

let run p =
  let p = Array.of_list p in
  let tape = Array.make 30_000 (Char.chr 0) in
  let rec f dp ip =
    if ip < Array.length p then
      match p.(ip) with
      | TapeInc x -> f (dp + x) (ip + 1)
      | TapeDec x -> f (dp - x) (ip + 1)
      | CellInc x ->
          tape.(dp) <- (Char.code tape.(dp) + x) mod 256 |> Char.chr;
          f dp (ip + 1)
      | CellDec x ->
          tape.(dp) <- imod (Char.code tape.(dp) - x) 256 |> Char.chr;
          f dp (ip + 1)
      | JumpIfZero ip' ->
          if tape.(dp) = Char.chr 0 then f dp (ip' + 1) else f dp (ip + 1)
      | JumpIfNotZero ip' ->
          if tape.(dp) <> Char.chr 0 then f dp (ip' + 1) else f dp (ip + 1)
      | Read ->
          tape.(dp) <- input_char stdin;
          f dp (ip + 1)
      | Write ->
          tape.(dp) |> print_char;
          f dp (ip + 1)
  in
  f 0 0
