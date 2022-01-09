open Stdint

(* function compisition f << g *)
let (<<) f g x = f (g x)

(* S combinator f >< g *)
let (><) f g x = f x (g x)
(* let (>|<) f g = ((><) f << Fun.const) >< g *)
(* let (>|<) f g x = f (g x) x *)
let (>|<) f g x = (><) f (Fun.const x) (g x)

(* esentially a typeclass. tells us how to read and write the type and its size *)
type 'a backing = {
  size : 'a -> int option;
  toBytes : Bytes.t -> int -> 'a -> int;
  fromBytes : Bytes.t -> int -> 'a;
}

(* the field in the data we are trying to represent *)
(* a chunk of data that has a unique backing *)
type _ field = Backed : ('a * 'a backing) -> 'a field [@@unboxed]

(* we need this existential type for heterogenous lists to describe our layouts *)
type any_field = Field : _ field -> any_field [@@unboxed]

(* we keep the backing types in a seperate namespace for convience and so we can expose smart constructors with the same names *)
module T = struct
  (* smart constructor for some backing *)
  let backing size toBytes fromBytes = {size; toBytes; fromBytes}

  (* smart constructor for constant sized "simple" types like ints and variant with no fields *)
  let mk_const s t f =
    (* can we be more succint here? *)
    let t' b i v = t b i v; s in
    backing (Fun.const (Some s)) t' f

  (* smart constructors for iterable types. NOTE: we do not suppotrt arbitralily long iterables OOTB.
       i.e. you would need to write the routines for a stream yourself *)
  (* we need this because we can't pass polymorhphics functions for some reason??? *)
  type ('e, 'c) iterT = {
    fold : 'a. ('a -> 'e -> 'a) -> 'a -> 'c -> 'a;
    init : int -> (int -> 'e) -> 'c
  }

  (* let mk_iter : type a c. (a, c) foldT -> (int -> (int -> a) -> c) -> (a backing -> int -> c backing) = *)
  let mk_iter =
    fun iter t (l: int) ->
    let size o =
      let f acc e =
        match acc with
        | None -> None
        | Some x -> Option.map ((+) x) (t.size e)
      in iter.fold f (Some 0) o in
    let toBytes b i v = iter.fold (fun acc x -> acc + t.toBytes b i x) 0 v in
    let fromBytes = fun b i -> iter.init l (fun idx -> t.fromBytes b (idx + i)) in
    { size; toBytes; fromBytes }

  let char = Bytes.(mk_const 1 set get)

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
    let iter = Array.{fold = fold_left; init} in
    mk_iter iter t l
  let list t l =
    let iter = List.{fold = fold_left; init} in
    mk_iter iter t l
  let string =
    let iter = String.{fold = fold_left; init} in
    mk_iter iter char
end

module StreamT = struct
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

let char = backed T.char

let u8 = backed T.u8 << Uint8.of_int
let u16_le = backed T.u16_le << Uint16.of_int
let u32_le = backed T.u32_le << Uint32.of_int
let u64_le = backed T.u64_le << Uint64.of_int
let u16_be = backed T.u16_be << Uint16.of_int
let u32_be = backed T.u32_be << Uint32.of_int
let u64_be = backed T.u64_be << Uint64.of_int

let i8 = backed T.i8 << Int8.of_int
let i16_le = backed T.i16_le << Int16.of_int
let i32_le = backed T.i32_le << Int32.of_int
let i64_le = backed T.i64_le << Int64.of_int
let i16_be = backed T.i16_be << Int16.of_int
let i32_be = backed T.i32_be << Int32.of_int
let i64_be = backed T.i64_be << Int64.of_int

let list t = backed >|< (T.list t << List.length)
let array t = backed >|< (T.array t << Array.length)
let string = backed >|< (T.string << String.length)

let get : type a. a field -> a =
  fun (Backed(b, _)) -> b

let set : type a. a field -> a -> a field =
  fun (Backed (_, t)) v -> Backed (v, t)

let read : type a. Bytes.t -> int -> a backing -> a field = fun b i t ->
  Backed(t.fromBytes b i, t)

let write : type a. Bytes.t -> int -> a field -> int =
  fun b i (Backed(v, t)) -> t.toBytes b i v

let sizeof : type a. a field -> int option =
  fun (Backed(v, b)) -> b.size v

let sizeofAny =
  let g acc (Field f) =
    match acc with
    | None -> None
    | Some x -> Option.map ((+) x) (sizeof f)
  in List.fold_left g (Some 0)
