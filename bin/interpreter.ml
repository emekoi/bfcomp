open Bfcomp.Ir

let imod a b =
  let m = a mod b in
  if m >= 0 then m else m + b

let run program =
  let prog = Array.of_list program in
  let tape = Array.make 30_000 (Char.chr 0) in
  let rec f p dp ip =
    if ip < Array.length p then (
      match p.(ip) with
      | Tape x -> f p (dp + x) (ip + 1)
      | Set x ->
          tape.(dp) <- Char.chr x;
          f p dp (ip + 1)
      | Cell x ->
          if x > 0 then
            tape.(dp) <- (Char.code tape.(dp) + x) mod 256 |> Char.chr
          else tape.(dp) <- imod (Char.code tape.(dp) + x) 256 |> Char.chr;
          f p dp (ip + 1)
      | Loop body ->
          let body = Array.of_list body in
          let rec loop dp' =
            if tape.(dp') <> Char.chr 0 then loop (f body dp' 0) else dp'
          in
          f p (loop dp) (ip + 1)
      | Read ->
          tape.(dp) <- input_char stdin;
          f p dp (ip + 1)
      | Write ->
          tape.(dp) |> print_char;
          f p dp (ip + 1))
    else dp
  in
  ignore (f prog 0 0)
