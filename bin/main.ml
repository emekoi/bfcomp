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
  "+[>[<->+[>+++>[+++++++++++>][]-[<]>-]]++++++++++<]>>>>>>----.<<+++.<-..+++.<-.>>>.<<.+++.------.>-.<<+.<.<"

(* let () =
  Parser.parse bf_program
  |> Ir.Pass.run [ Ir.Pass.contraction ]
  |> Interpreter.run *)
(* |> List.iter (fun x -> print_endline (Ir.show x)) *)

let rec show_t = function
  | Ir.Tape x -> String.make (abs x) (if x < 0 then '<' else '>')
  | Ir.Cell x -> String.make (abs x) (if x < 0 then '-' else '+')
  | Ir.Set _ -> "[-]"
  | Ir.Read -> ","
  | Ir.Write -> "."
  | Ir.Loop x -> "[" ^ (List.map show_t x |> String.concat "") ^ "]"

let rec show_instr = function
  | Ir.Tape x -> "Tape " ^ string_of_int x ^ "\n"
  | Ir.Cell x -> "Cell " ^ string_of_int x ^ "\n"
  | Ir.Set x -> "Set " ^ string_of_int x ^ "\n"
  | Ir.Read -> "Read\n"
  | Ir.Write -> "Write\n"
  | Ir.Loop x ->
      "Loop [\n  " ^ (List.map show_instr x |> String.concat "  ") ^ "]\n"

let main () =
  (if Array.length Sys.argv > 1 then
   open_in_bin Sys.argv.(1) |> string_of_in_channel
  else hello_word)
  |> Parser.parse
  |> Ir.Pass.run [ Ir.Pass.contraction; Ir.Pass.dead_code; Ir.Pass.zero_loop ]
  |> List.map show_t |> String.concat "" |> print_endline
(* |> Interpreter.run *)

let () = main ()
