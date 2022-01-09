open Stdint

type 'a size =
  | Static: int -> _ size
  | Dynamic: ('a -> int option) -> 'a size

type 'a backing = {
  size : 'a size;
  toBytes : Bytes.t -> int -> 'a -> int;
  fromBytes : Bytes.t -> int -> 'a;
}

type _ field
type any_field

module T :  sig
  type ('e, 'c) iterT = {
    fold : 'a. ('a -> 'e -> 'a) -> 'a -> 'c -> 'a;
    init : int -> (int -> 'e) -> 'c;
  }

  val backing : 'a size -> (Bytes.t -> int -> 'a -> int) -> (Bytes.t -> int -> 'a) -> 'a backing
  val mk_const : int -> (Bytes.t -> int -> 'a -> unit) -> (Bytes.t -> int -> 'a) -> 'a backing
  val mk_iter : ('a, 'b) iterT -> 'a backing -> int -> 'b backing
  val mk_int : int -> ('a -> Bytes.t -> int -> unit) -> (Bytes.t -> int -> 'a) -> 'a backing

  val char : char backing
  val u8 : Uint8.t backing
  val u16_le : Uint16.t backing
  val u32_le : Uint32.t backing
  val u64_le : Uint64.t backing
  val u16_be : Uint16.t backing
  val u32_be : Uint32.t backing
  val u64_be : Uint64.t backing

  val i8 : Int8.t backing
  val i16_le : Int16.t backing
  val i32_le : Int32.t backing
  val i64_le : Int64.t backing
  val i16_be : Int16.t backing
  val i32_be : Int32.t backing
  val i64_be : Int64.t backing

  val array : 'a backing -> int -> 'a array backing
  val list : 'a backing -> int -> 'a list backing
  val string : int -> string backing
end

module StreamT : sig
  type 'a t
  val stream : 'a backing -> (int -> Bytes.t -> int -> 'a option) -> 'a t backing
  val streamCount : 'a backing -> int -> 'a t backing
  val streamUntil : 'a backing -> ('a -> bool) -> 'a t backing
end

val backed : 'a backing -> 'a -> any_field

val char : char -> any_field

val u8 : int -> any_field
val u16_le : int -> any_field
val u32_le : int -> any_field
val u64_le : int -> any_field
val u16_be : int -> any_field
val u32_be : int -> any_field
val u64_be : int -> any_field

val i8 : int -> any_field
val i16_le : int -> any_field
val i32_le : int -> any_field
val i64_le : int -> any_field
val i16_be : int -> any_field
val i32_be : int -> any_field
val i64_be : int -> any_field

val list : 'a backing -> 'a list -> any_field
val array : 'a backing -> 'a array -> any_field
val string : string -> any_field

val get : 'a field -> 'a
val set : 'a field -> 'a -> 'a field
val write : Bytes.t -> int -> 'a field -> int
val read : Bytes.t -> int -> 'a backing -> 'a field
val sizeof : any_field -> int option
