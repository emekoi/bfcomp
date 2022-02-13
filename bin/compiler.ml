open Bfcomp
open Codegen

type t = { buf : Bytebuffer.t; sp: Codegen.reg }

let op_of_ir ctx = function
  | Ir.Set arg -> Mov(reg ctx.sp, imm8 arg)
  | _ -> failwith "TODO"
