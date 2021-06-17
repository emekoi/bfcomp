open Bfcomp

let bf_program =
  ">+++++++++[<++++++++>-]<.>++++++[<+++++>-]<-.+++++++..+++.>>+++++++[<++++++>-]<++.------------.<++++++++.--------.+++.------.--------.>+.>++++++++++."

let () =
  Parser.parse bf_program
  |> Ir.Pass.run [ Ir.Pass.contraction ]
  |> List.iter (fun x -> print_endline (Ir.show x))
