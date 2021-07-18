open Stdint

type t

val empty : t
val copy : t -> t -> unit
val length : t -> int

val set_char : t -> char -> unit
val set_string : t -> string -> unit
val set_i8 : t -> int8 -> unit
val set_u8 : t -> uint8 -> unit
val set_i16 : t -> int16 -> unit
val set_u16 : t -> uint16 -> unit
val set_i32 : t -> int32 -> unit
val set_u32 : t -> uint32 -> unit
val set : t -> uint8 list -> unit
val pad : t -> int -> char -> unit
val pad_u8 : t -> int -> int -> unit
