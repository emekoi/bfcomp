open Stdint

type t = { mutable idx : int; buf : Bytes.t }

let empty = { idx = 0; buf = Bytes.empty }

let copy dst src =
  Bytes.blit src.buf 0 dst.buf (dst.idx + 1) src.idx;
  dst.idx <- dst.idx + src.idx

let length { idx; _ } = idx

let set_char ({ buf; idx } as c) v =
  Bytes.set buf idx v;
  c.idx <- c.idx + 1

let set_string ({ buf; idx } as c) s =
  let length = String.length s in
  Bytes.blit_string s 0 buf idx length;
  c.idx <- c.idx + length

let set_i8 ({ buf; idx } as c) v =
  Int8.to_bytes_little_endian v buf idx;
  c.idx <- c.idx + 1

let set_u8 ({ buf; idx } as c) v =
  Uint8.to_bytes_little_endian v buf idx;
  c.idx <- c.idx + 1

let set_i16 ({ buf; idx } as c) v =
  Int16.to_bytes_little_endian v buf idx;
  c.idx <- c.idx + (Int16.bits / 8)

let set_u16 ({ buf; idx } as c) v =
  Uint16.to_bytes_little_endian v buf idx;
  c.idx <- c.idx + (Uint16.bits / 8)

let set_i32 ({ buf; idx } as c) v =
  Int32.to_bytes_little_endian v buf idx;
  c.idx <- c.idx + (Int32.bits / 8)

let set_u32 ({ buf; idx } as c) v =
  Uint32.to_bytes_little_endian v buf idx;
  c.idx <- c.idx + (Uint32.bits / 8)

let set c list = List.rev list |> List.iter (set_u8 c)

let pad ({ buf; idx } as c) l v =
  Bytes.fill buf idx l v;
  c.idx <- c.idx + l

let pad_u8 ({ buf; idx } as c) l v =
  Bytes.fill buf idx l (Char.chr v);
  c.idx <- c.idx + l
