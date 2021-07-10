type t = { mutable idx : int; buf : Bytes.t }

let set_char ({ buf; idx } as c) v =
  Bytes.set buf idx v;
  c.idx <- c.idx + 1

let set_string ({ buf; idx } as c) s =
  let length = String.length s in
  Bytes.blit_string s 0 buf idx length;
  c.idx <- c.idx + length

let set_u8 ({ buf; idx } as c) v =
  Bytes.set_uint8 buf idx v;
  c.idx <- c.idx + 1

let set_u16 ({ buf; idx } as c) v =
  Bytes.set_uint16_le buf idx v;
  c.idx <- c.idx + 1

let set_u32 ({ buf; idx } as c) v =
  Bytes.set_int32_le buf idx v;
  c.idx <- c.idx + 1

let pad ({ buf; idx } as c) l v =
  Bytes.fill buf idx l v;
  c.idx <- c.idx + l

let pad_u8 ({ buf; idx } as c) l v =
  Bytes.fill buf idx l (Char.chr v);
  c.idx <- c.idx + l
