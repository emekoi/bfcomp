(* I: id *)
let id = Fun.id

(* K: constant *)
let const = Fun.const

(* A: application *)
let ($) f x = f x

(* A' *)
let (&) x f = f x

(* W: duplication *)
let join f x = f x x

(* C: flip *)
let flip f = fun x y -> f y x

(* B: compose *)
let (<<) f g x = f (g x)

(* S: substitution *)
let (<*>) f g x = f x (g x)

(* S': chain (S reversed) *)
let (>*<) f g x = f (g x) x

(* S2: converge. like S, but with 2 functions. *)
let converge f g h x = f (g x) (h x)

(* P: psi/on *)
let on b f x y = b (f x) (f y)

(* Z: Z fixed point since ocaml is eager*)
let rec fix f x = f (fix f) x
