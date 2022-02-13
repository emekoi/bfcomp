open Stdint

type t

type reg = Eax | Ecx | Edx | Ebx | Esp | Ebp | Esi | Edi
val int_of_reg : reg -> uint8

type _ op =
  | Reg : reg -> reg op
  | Mem : 'a op -> 'a op
  | Imm8 : int8 -> int8 op
  | Uimm8 : uint8 -> uint8 op
  | Imm16 : int16 -> int16 op
  | Uimm16 : uint16 -> uint16 op
  | Imm32 : int32 -> int32 op
  | Uimm32 : uint32 -> uint32 op

val reg : reg -> reg op
val mem : 'a op -> 'a op
val imm8 : int -> int8 op
val uimm8 : int -> uint8 op
val imm16 : int -> int16 op
val uimm16 : int -> uint16 op
val imm32 : int -> int32 op
val uimm32 : int -> uint32 op
val emit_op : t -> 'a op -> unit

type any_op = Op : _ op -> any_op [@@unboxed]

val encode_int : int -> any_op
val encode_uint : int -> any_op
val align: int -> int -> int

type 'a jmp = JMP of 'a | JZ of 'a | JNZ of 'a

type _ instr =
  | Mov : 'a op * 'b op -> ('a * 'b) instr
  | Add : 'a op * 'b op -> ('a * 'b) instr
  | Sub : 'a op * 'b op -> ('a * 'b) instr
  | Cmp : 'a op * 'b op -> ('a * 'b) instr
  | Intr : uint8 -> uint8 instr
  | Jmp : 'a jmp -> 'a jmp instr

val emit_instr : t -> 'a instr -> unit
val emit_loop : t -> (t -> unit) -> unit
val emit_syscall : t -> uint16 -> 'a op list -> unit
