open Bfcomp

let string_of_in_channel i =
  let buf = Buffer.create 1024 in
  let line_stream_of_channel channel =
    Stream.from (fun _ ->
        try Some (input_line channel) with End_of_file -> None)
  in

  Stream.iter (Buffer.add_string buf) (line_stream_of_channel i);
  Buffer.contents buf

let hello_word =
  ">+++++++++[<++++++++>-]<.>++++++[<+++++>-]<-.+++++++..+++.>>+++++++[<++++++>-]<++.------------.<++++++++.--------.+++.------.--------.>+.>++++++++++."

(* let () =
  Parser.parse bf_program
  |> Ir.Pass.run [ Ir.Pass.contraction ]
  |> Interpreter.run *)
(* |> List.iter (fun x -> print_endline (Ir.show x)) *)

let main () =
  (if Array.length Sys.argv > 1 then
   open_in_bin Sys.argv.(1) |> string_of_in_channel
  else hello_word)
  |> Parser.parse
  |> Ir.Pass.run [ Ir.Pass.contraction ]
  |> Interpreter.run

let () = main ()
