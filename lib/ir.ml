type t =
  | (* > *) TapeInc of int
  | (* < *) TapeDec of int
  | (* + *) CellInc of int
  | (* - *) CellDec of int
  | (* , *) Read
  | (* . *) Write
  | (* [ *) JumpIfZero of int
  | (* ] *) JumpIfNotZero of int
[@@deriving show]

module Pass = struct
  let contraction =
    let g = function
      | TapeInc a as b -> if a < 0 then TapeDec (-a) else b
      | CellInc a as b -> if a < 0 then CellDec (-a) else b
      | i -> i
      (* | TapeDec a as b -> if a < 0 then TapeInc (-a) else b *)
      (* | CellDec a as b -> if a < 0 then CellDec (-a) else b *)
    in
    let f x = function
      | [] -> [ x ]
      | x' :: t as l -> (
          match (x, x') with
          | TapeDec a, TapeDec b -> TapeDec (a + b) :: t
          | TapeInc a, TapeInc b -> TapeInc (a + b) :: t
          | CellDec a, CellDec b -> CellDec (a + b) :: t
          | CellInc a, CellInc b -> CellInc (a + b) :: t
          | CellInc a, CellDec b | CellDec b, CellInc a ->
              g (CellInc (a - b)) :: t
          | TapeInc a, TapeDec b | TapeDec b, TapeInc a ->
              g (TapeInc (a - b)) :: t
          | _ -> x :: l)
    in
    List.fold_left (Fun.flip f) []

  let finalize l =
    let stack = Stack.create () in
    let array = List.rev l |> Array.of_list in
    Array.iteri
      (fun i -> function
        | JumpIfZero _ -> Stack.push i stack
        | JumpIfNotZero _ ->
            let idx = Stack.pop stack in
            print_endline (string_of_int i);
            array.(idx) <- JumpIfZero i;
            array.(i) <- JumpIfNotZero idx
        | _ -> ())
      array;
    Array.to_list array

  let run passes l = List.fold_left (fun l p -> p l) l passes |> finalize
end
