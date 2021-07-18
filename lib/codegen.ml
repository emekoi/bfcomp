open Stdint

type t = { buf : Bytebuffer.t; mutable length : int }

(* esp = data pointer *)
type reg = Eax | Ecx | Edx | Ebx | Esp | Ebp | Esi | Edi

let int_of_reg = function
  | Eax -> Uint8.of_int 0b000
  | Ecx -> Uint8.of_int 0b001
  | Edx -> Uint8.of_int 0b010
  | Ebx -> Uint8.of_int 0b011
  | Esp -> Uint8.of_int 0b100
  | Ebp -> Uint8.of_int 0b101
  | Esi -> Uint8.of_int 0b110
  | Edi -> Uint8.of_int 0b111

type _ op =
  | Reg : reg -> reg op
  | Mem : 'a op -> 'a op op
  | Imm8 : int8 -> int8 op
  | Uimm8 : uint8 -> uint8 op
  | Imm16 : int16 -> int16 op
  | Uimm16 : uint16 -> uint16 op
  | Imm32 : int32 -> int32 op
  | Uimm32 : uint32 -> uint32 op

type any_op = Op : _ op -> any_op [@@unboxed]

let imm8 v = Imm8 (Int8.of_int v)

let uimm8 v = Uimm8 (Uint8.of_int v)

let imm16 v = Imm16 (Int16.of_int v)

let uimm16 v = Uimm16 (Uint16.of_int v)

let imm32 v = Imm32 (Int32.of_int v)

let uimm32 v = Uimm32 (Uint32.of_int v)

let emit_op b =
  let open Bytebuffer in
  let rec f : type a. a op -> unit = function
    | Reg r -> int_of_reg r |> set_u8 b.buf
    | Mem m -> f m
    | Imm8 i -> set_i8 b.buf i
    | Uimm8 i -> set_u8 b.buf i
    | Imm16 i -> set_i16 b.buf i
    | Uimm16 i -> set_u16 b.buf i
    | Imm32 i -> set_i32 b.buf i
    | Uimm32 i -> set_u32 b.buf i
  in
  f

type 'a jmp = JMP of 'a | JZ of 'a | JNZ of 'a

type _ instr =
  | Mov : 'a op * 'b op -> ('a * 'b) instr
  | Add : 'a op * 'b op -> ('a * 'b) instr
  | Sub : 'a op * 'b op -> ('a * 'b) instr
  | Cmp : 'a op * 'b op -> ('a * 'b) instr
  | Intr : uint8 -> uint8 instr
  | Jmp : 'a jmp -> 'a jmp instr

let rec log2 n = if n <= 1 then 0 else 1 + log2 (n asr 1)

let align n a = if n >= 0 then (n + a - 1) land -a else n land -a

let encode_int : int -> any_op =
 fun v ->
  let s = if v > 0 then 1 else if v = 0 then 0 else -1 in
  match log2 (abs v) with
  | v when v >= 0 && v < 8 -> Op (imm8 (v * s))
  | v when v >= 8 && v < 16 -> Op (imm16 (v * s))
  | v when v >= 16 && v < 32 -> Op (imm32 (v * s))
  | _ -> invalid_arg (string_of_int v ^ " is too large")

let encode_uint : int -> any_op =
 fun v ->
  match log2 (abs v) with
  | v when v >= 0 && v < 8 -> Op (uimm8 v)
  | v when v >= 8 && v < 16 -> Op (uimm16 v)
  | v when v >= 16 && v < 32 -> Op (uimm32 v)
  | _ -> invalid_arg (string_of_int v ^ " is too large")

let emit_instr : type a. t -> a instr -> unit =
 fun b ->
  let () = b.length <- b.length + 1 in
  function
  | Mov (_op1, _op2) -> ()
  | Add (_op1, _op2) -> ()
  | Sub (_op1, _op2) -> ()
  | Cmp (_op1, _op2) -> ()
  | Intr _i -> ()
  | Jmp _i -> ()

let emit_loop b gen_body =
  (*
  jmp loop_cnd (loop.length + 1)
  loop_start:
    gen_loop
  loop_cnd:
    test esp, esp
    jnz loop_start -(loop.length + 1)
  *)
  let loop = { b with buf = Bytebuffer.empty } in
  gen_body loop;
  emit_instr loop (Cmp (Mem (Reg Esp), imm8 0));

  let loop_cnd = loop.length + 1 in
  emit_instr b (Jmp (JMP loop_cnd));
  Bytebuffer.copy b.buf loop.buf;

  let loop_start = -loop_cnd in
  emit_instr b (Jmp (JNZ loop_start))

let emit_syscall : type a. t -> uint16 -> a op list -> unit =
 fun b sys args ->
  if List.length args > 5 then
    raise (Invalid_argument "too many args for syscall")
  else
    let arg_regs = [ Reg Ebx; Reg Ecx; Reg Edx; Reg Esi; Reg Edi ] in
    let rec zip f a b =
      match (a, b) with
      | [], _ -> []
      | _, [] -> []
      | x :: a', y :: b' -> f x y :: zip f a' b'
    in
    let reg_set = zip (fun x y -> Mov (x, y)) args arg_regs in
    List.iter (emit_instr b) reg_set;
    emit_instr b (Mov (Uimm16 sys, Reg Eax));
    emit_instr b (Intr (Uint8.of_int 0x80))
