open Stdint

(* function compisition f << g *)
let (<<) f g x = f (g x)

(* S combinator f >< g *)
let (><) f g x = f x (g x)
let (>|<) f g x = f (g x) x

(* esentially a typeclass. tells us how to read and write the type and its size *)
type 'a backing = {
  size : 'a -> int option;
  toBytes : Bytes.t -> int -> 'a -> int;
  fromBytes : Bytes.t -> int -> 'a;
}

(* the field in the data we are trying to represent *)
type _ field =
  | Backed : ('a * 'a backing) -> ('a * 'a backing) field (* a chunk of data that has a unique backing *)
  | Range : int * int -> (int * int) field (* aliases some other region of our data *)
  | Bytes : int -> int field               (* some opaque block of memory we don't care about *)

(* we need this existential type for heterogenous lists to describe our layouts *)
type any_field = Field : _ field -> any_field [@@unboxed]

(* module TODO = struct *)
(*   (\* we want to abstract away types that can be iterated on *\) *)
(*   module type IterableT = sig *)
(*     type t *)
(*     type e *)
(*     val fold : (int -> e -> int) -> int -> t -> int *)
(*     val iter : (e -> unit) -> t -> unit *)
(*     val init : int -> (int -> e) -> t *)
(*   end *)

(*   (\* this is functor takes a module and returns a struct with the function *)
(*      we need to read/write and get length info about the module *\) *)
(*   module MakeIterable (T: IterableT) = struct *)
(*     let mk_iter = fun t (l: int) -> *)
(*       let size v = T.fold (fun acc x -> acc + t.size x) 0 v in *)
(*       let toBytes b i v = T.iter (t.toBytes b i) v in *)
(*       let fromBytes = fun b i -> T.init l (fun idx -> t.fromBytes b (idx + i)) in *)
(*       { size; toBytes; fromBytes } *)
(*   end *)
(*   module StringT = MakeIterable(struct *)
(*       type t = string *)
(*       type e = char *)
(*       let fold = String.fold_left *)
(*       let iter = String.iter *)
(*       let init = String.init *)
(*     end) *)
(*   let string = StringT.mk_iter *)
(* end *)

(* we keep the backing types in a seperate namespace for convience and so we can expose smart constructors with the same names *)
module T = struct
  (* smart constructor for some backing *)
  let backing size toBytes fromBytes = {size; toBytes; fromBytes}

  (* smart constructor for constant sized "simple" types like ints and variant with no fields *)
  let mk_const s t f =
    (* can we be more succint here? *)
    let t' b i v = t b i v; Option.value s ~default:0 in
    backing (Fun.const s) t' f

  (* smart constructors for iterable types. NOTE: we do not suppotrt arbitralily long iterables OOTB.
       i.e. you would need to write the routines for a stream yourself *)
  (* type ('a, 'b, 'c) foldT = ('a -> 'b -> 'a) -> 'a -> 'c *)
  (* let mk_iter (fold :('a -> 'b -> 'a) -> 'a -> 'c) iter init = *)
  (* let mk_iter : type ce c. (('a -> ce -> 'a) -> 'a -> c -> 'a) -> (int -> (int -> ce) -> c) -> (ce backing -> int -> c backing) = *)
  type ('e, 'c) foldT = {fold : 'a. ('a -> 'e -> 'a) -> 'a -> 'c -> 'a}
  (* because we can't pass polymorhphics functions for some reason??? *)
  let mk_iter : type a c. (a, c) foldT -> (int -> (int -> a) -> c) -> (a backing -> int -> c backing) =
    fun fold init t (l: int) ->
    let size o =
      let f acc e =
        match acc with
        | None -> None
        | Some x -> Option.map ((+) x) (t.size e)
      in fold.fold f (Some 0) o in
    (* let size v = *)
    (*   fold (fun acc x -> acc + t.size x) 0 v in *)
    let toBytes b i v = fold.fold (fun acc x -> acc + t.toBytes b i x) 0 v in
    let fromBytes = fun b i -> init l (fun idx -> t.fromBytes b (idx + i)) in
    { size; toBytes; fromBytes }

  (* can we be more succint here? *)
  let char = Bytes.(mk_const (Some 1) set get)

  let mk_int s t f =
    let t' b i v = t v b i; s in
    backing (Fun.const (Some s)) t' f

  let u8 = Uint8.(mk_int 1 to_bytes_little_endian of_bytes_little_endian)
  let u16_le = Uint16.(mk_int 2 to_bytes_little_endian of_bytes_little_endian)
  let u32_le = Uint32.(mk_int 4 to_bytes_little_endian of_bytes_little_endian)
  let u64_le = Uint64.(mk_int 8 to_bytes_little_endian of_bytes_little_endian)
  let u16_be = Uint16.(mk_int 2 to_bytes_big_endian of_bytes_big_endian)
  let u32_be = Uint32.(mk_int 4 to_bytes_big_endian of_bytes_big_endian)
  let u64_be = Uint64.(mk_int 8 to_bytes_big_endian of_bytes_big_endian)

  let i8 = Int8.(mk_int 1 to_bytes_little_endian of_bytes_little_endian)
  let i16_le = Int16.(mk_int 2 to_bytes_little_endian of_bytes_little_endian)
  let i32_le = Int32.(mk_int 4 to_bytes_little_endian of_bytes_little_endian)
  let i64_le = Int64.(mk_int 8 to_bytes_little_endian of_bytes_little_endian)
  let i16_be = Int16.(mk_int 2 to_bytes_big_endian of_bytes_big_endian)
  let i32_be = Int32.(mk_int 4 to_bytes_big_endian of_bytes_big_endian)
  let i64_be = Int64.(mk_int 8 to_bytes_big_endian of_bytes_big_endian)

  (* i'm not quite why, but if we eta reduce these it does not type check *)
  let array t l =
    let fold = {fold = Array.fold_left} in
    Array.(mk_iter fold init) t l
  let list t l =
    let fold = {fold = List.fold_left} in
    List.(mk_iter fold init) t l
  let string =
    let fold = {fold = String.fold_left} in
    String.(mk_iter fold init) char
end

module Unsized = struct
  type 'a t = {
    mutable size: int option;
    stream: 'a Stream.t
  }

  let streamSize s = s.size

  let streamToBytes t b i s =
    let idx = ref 0 in
    let f e =
      let written = t.toBytes b (!idx + i) e in
      idx := !idx + written in
    Stream.iter f s.stream;
    s.size <- Some !idx;
    !idx

  let streamOfBytes (t : 'a backing) from = fun b i ->
    let idx = ref 0 in
    let f si =
      match from si b (!idx + i) with
      | None -> None
      | Some e ->
        idx := Option.fold ~none:!idx ~some:((+) !idx) (t.size e);
        Some e
    in {size = Some !idx; stream = Stream.from f}

  let stream t f =
    {size = streamSize; toBytes = streamToBytes t; fromBytes = streamOfBytes t f}

  let streamCount t n =
    let from si b idx = if si < n then Some (t.fromBytes b idx) else None in
    stream t from

  let streamUntil t p =
    let from _ b idx =
      let e = t.fromBytes b idx in
      if p e then None else Some e
    in stream t from
end

let backed t v = Field (Backed (v, t))
let range x y = Field (Range(x, y))
(* let bytes x = Field (Bytes x) *)

let char = backed T.char
let u8 = backed T.u8 << Uint8.of_int
let u16_le = backed T.u16_le << Uint16.of_int
let u32_le = backed T.u32_le << Uint32.of_int
let u64_le = backed T.u64_le << Uint64.of_int
let u16_be = backed T.u16_be << Uint16.of_int
let u32_be = backed T.u32_be << Uint32.of_int
let u64_be = backed T.u64_be << Uint64.of_int
let i16_le = backed T.i16_le << Int16.of_int
let i32_le = backed T.i32_le << Int32.of_int
let i64_le = backed T.i64_le << Int64.of_int
let i16_be = backed T.i16_be << Int16.of_int
let i32_be = backed T.i32_be << Int32.of_int
let i64_be = backed T.i64_be << Int64.of_int

let list t = backed >|< (T.list t << List.length)
let array t = backed >|< (T.array t << Array.length)
let string = backed >|< (T.string << String.length)

let size : type a. a field -> int option = function
  | Range _ -> Some 0
  | Bytes x -> Some x
  | Backed(v, b) -> b.size v

let sizeof =
  let g acc (Field f) =
    match acc with
    | None -> None
    | Some x -> Option.map ((+) x) (size f)
  in List.fold_left g (Some 0)

let write =
  let g acc (Field f) =
    match acc with
    | None -> None
    | Some x -> Option.map ((+) x) (size f)
  in List.fold_left g (Some 0)


let elf_header = [
  u8 0x7f;
  string "ELF";
]

let size_elf = sizeof elf_header
